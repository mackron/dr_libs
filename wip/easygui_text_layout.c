// Public domain. See "unlicense" statement at the end of this file.

#include "easygui_text_layout.h"
#include <math.h>
#include <assert.h>

#ifndef OUT
#define OUT
#endif

#ifndef DO_NOTHING
#define DO_NOTHING
#endif

typedef struct
{
    /// The index of the run within the line the marker is positioned on.
    unsigned int iRun;

    /// The index of the character within the run the marker is positioned to the left of.
    unsigned int iChar;

    /// The position on the x axis, relative to the x position of the run.
    float relativePosX;

    /// The absolute position on the x axis to place the marker when moving up and down lines. Note that this is not relative
    /// to the run, but rather the line. This will be updated when the marker is moved left and right.
    float absoluteSickyPosX;

} easygui_text_marker;

/// Keeps track of the current state of the text layout. Used for calculating the difference between two states for undo/redo.
typedef struct
{
    /// The text. Can be null in some cases where it isn't used.
    char* text;

    /// The index of the character the cursor is positioned at.
    size_t cursorPos;

    /// The index of the character the selection anchor is positioned at.
    size_t selectionAnchorPos;

    /// Whether or not anything is selected.
    bool isAnythingSelected;

} easygui_text_layout_state;

typedef struct
{
    /// The position in the main string where the change is located. The length of the relevant string is used to determines how
    /// large of a chunk of text needs to be replaced.
    size_t diffPos;

    /// The string that was replaced. On undo, this will be inserted into the text layout. Can be empty, in which case this state
    /// object was created in response to an insert operation.
    char* oldText;

    /// The string that replaces the old text. On redo, this will be inserted into the text layout. This can be empty, in which case
    /// this state object was created in response to a delete operation.
    char* newText;

    /// The state of the text layout at the time the undo point was prepared, not including the text. The <text> attribute
    /// of this object is always null.
    easygui_text_layout_state oldState;

    /// The state of the text layout at the time the undo point was committed, not including the text. The <text> attribute
    /// of this object is always null.
    easygui_text_layout_state newState;

} easygui_text_layout_undo_state;

struct easygui_text_layout
{
    /// The main text of the layout.
    char* text;

    /// The length of the text.
    size_t textLength;


    /// The function to call when the text layout needs to be redrawn.
    easygui_text_layout_on_dirty_proc onDirty;


    /// The width of the container.
    float containerWidth;

    /// The height of the container.
    float containerHeight;

    /// The inner offset of the container.
    float innerOffsetX;

    /// The inner offset of the container.
    float innerOffsetY;


    /// The default font.
    easygui_font* pDefaultFont;

    /// The default text color.
    easygui_color defaultTextColor;

    /// The default background color.
    easygui_color defaultBackgroundColor;

    /// The background color to use for selected text.
    easygui_color selectionBackgroundColor;

    /// The size of a tab in spaces.
    unsigned int tabSizeInSpaces;

    /// The horizontal alignment.
    easygui_text_layout_alignment horzAlign;

    /// The vertical alignment.
    easygui_text_layout_alignment vertAlign;

    /// The width of the text cursor.
    float cursorWidth;

    /// The color of the text cursor.
    easygui_color cursorColor;
    

    /// The total width of the text.
    float textBoundsWidth;

    /// The total height of the text.
    float textBoundsHeight;


    /// The cursor.
    easygui_text_marker cursor;

    /// The selection anchor.
    easygui_text_marker selectionAnchor;


    /// The selection mode counter. When this is greater than 0 we are in selection mode, otherwise we are not. This
    /// is incremented by enter_selection_mode() and decremented by leave_selection_mode().
    unsigned int selectionModeCounter;

    /// Whether or not anything is selected.
    bool isAnythingSelected;


    /// The function to call when a text run needs to be painted.
    easygui_text_layout_on_paint_text_proc onPaintText;

    /// The function to call when a rectangle needs to be painted.
    easygui_text_layout_on_paint_rect_proc onPaintRect;

    /// The function to call when the cursor moves.
    easygui_text_layout_on_cursor_move_proc onCursorMove;


    /// The prepared undo/redo state. This will be filled with some state by PrepareUndoRedoPoint() and again with CreateUndoRedoPoint().
    easygui_text_layout_state preparedState;

    /// The undo/redo stack.
    easygui_text_layout_undo_state* pUndoStack;

    /// The number of items in the undo/redo stack.
    unsigned int undoStackCount;

    /// The index of the undo/redo state item we are currently sitting on.
    unsigned int iUndoState;


    /// A pointer to the buffer containing details about every run in the layout.
    easygui_text_run* pRuns;

    /// The number of runs in <pRuns>.
    size_t runCount;

    /// The size of the <pRuns> buffer in easygui_text_run's. This is used to determine whether or not the buffer
    /// needs to be reallocated upon adding a new run.
    size_t runBufferSize;


    /// The size of the extra data.
    size_t extraDataSize;

    /// A pointer to the extra data.
    char pExtraData[1];
};

/// Structure containing information about a line. This is used by first_line() and next_line().
typedef struct 
{
    /// The index of the line.
    unsigned int index;

    /// The position of the line on the y axis.
    float posY;

    /// The height of the line.
    float height;

    /// The index of the first run on the line.
    unsigned int iFirstRun;

    /// The index of the last run on the line.
    unsigned int iLastRun;

} easygui_text_layout_line;


/// Performs a complete refresh of the given text layout.
///
/// @remarks
///     This will delete every run and re-create them.
PRIVATE void easygui_text_layout__refresh(easygui_text_layout* pTL);

/// Refreshes the alignment of the given text layout.
PRIVATE void easygui_text_layout__refresh_alignment(easygui_text_layout* pTL);

/// Appends a text run to the list of runs in the given text layout.
PRIVATE void easygui_text_layout__push_text_run(easygui_text_layout* pTL, easygui_text_run* pRun);

/// Clears the internal list of text runs.
PRIVATE void easygui_text_layout__clear_text_runs(easygui_text_layout* pTL);

/// Helper for calculating the offset to apply to each line based on the alignment of the given text layout.
PRIVATE void easygui_text_layout__calculate_line_alignment_offset(easygui_text_layout* pTL, float lineWidth, float* pOffsetXOut, float* pOffsetYOut);

/// Helper for determine whether or not the given text run is whitespace.
PRIVATE bool easygui_text_layout__is_text_run_whitespace(easygui_text_layout* pTL, easygui_text_run* pRun);

/// Helper for calculating the width of a tab.
PRIVATE float easygui_text_layout__get_tab_width(easygui_text_layout* pTL);


/// Finds the line that's closest to the given point relative to the text.
PRIVATE bool easygui_text_layout__find_closest_line_to_point(easygui_text_layout* pTL, float inputPosYRelativeToText, unsigned int* pFirstRunIndexOnLineOut, unsigned int* pLastRunIndexOnLinePlus1Out);

/// Finds the run that's closest to the given point relative to the text.
PRIVATE bool easygui_text_layout__find_closest_run_to_point(easygui_text_layout* pTL, float inputPosXRelativeToText, float inputPosYRelativeToText, unsigned int* pRunIndexOut);

/// Retrieves some basic information about a line, namely the index of the last run on the line, and the line's height.
PRIVATE bool easygui_text_layout__find_line_info(easygui_text_layout* pTL, unsigned int iFirstRunOnLine, unsigned int* pLastRunIndexOnLinePlus1Out, float* pLineHeightOut);

/// Retrieves some basic information about a line by it's index.
PRIVATE bool easygui_text_layout__find_line_info_by_index(easygui_text_layout* pTL, unsigned int iLine, easygui_rect* pRectOut, unsigned int* pFirstRunIndexOut, unsigned int* pLastRunIndexPlus1Out);

/// Finds the last run on the line that the given run is sitting on.
PRIVATE bool easygui_text_layout__find_last_run_on_line_starting_from_run(easygui_text_layout* pTL, unsigned int iRun, unsigned int* pLastRunIndexOnLineOut);

/// Finds the first run on the line that the given run is sitting on.
PRIVATE bool easygui_text_layout__find_first_run_on_line_starting_from_run(easygui_text_layout* pTL, unsigned int iRun, unsigned int* pFirstRunIndexOnLineOut);

/// Finds the run containing the character at the given index.
PRIVATE bool easygui_text_layout__find_run_at_character(easygui_text_layout* pTL, unsigned int iChar, unsigned int* pRunIndexOut);


/// Creates a blank text marker.
PRIVATE easygui_text_marker easygui_text_layout__new_marker();

/// Moves the given text marker to the given point, relative to the container.
PRIVATE bool easygui_text_layout__move_marker_to_point_relative_to_container(easygui_text_layout* pTL, easygui_text_marker* pMarker, float inputPosX, float inputPosY);

/// Retrieves the position of the given text marker relative to the container.
PRIVATE void easygui_text_layout__get_marker_position_relative_to_container(easygui_text_layout* pTL, easygui_text_marker* pMarker, float* pPosXOut, float* pPosYOut);

/// Moves the marker to the given point, relative to the text rectangle.
PRIVATE bool easygui_text_layout__move_marker_to_point(easygui_text_layout* pTL, easygui_text_marker* pMarker, float inputPosXRelativeToText, float inputPosYRelativeToText);

/// Moves the given marker to the left by one character.
PRIVATE bool easygui_text_layout__move_marker_left(easygui_text_layout* pTL, easygui_text_marker* pMarker);

/// Moves the given marker to the right by one character.
PRIVATE bool easygui_text_layout__move_marker_right(easygui_text_layout* pTL, easygui_text_marker* pMarker);

/// Moves the given marker up one line.
PRIVATE bool easygui_text_layout__move_marker_up(easygui_text_layout* pTL, easygui_text_marker* pMarker);

/// Moves the given marker down one line.
PRIVATE bool easygui_text_layout__move_marker_down(easygui_text_layout* pTL, easygui_text_marker* pMarker);

/// Moves the given marker to the end of the line it's currently sitting on.
PRIVATE bool easygui_text_layout__move_marker_to_end_of_line(easygui_text_layout* pTL, easygui_text_marker* pMarker);

/// Moves the given marker to the start of the line it's currently sitting on.
PRIVATE bool easygui_text_layout__move_marker_to_start_of_line(easygui_text_layout* pTL, easygui_text_marker* pMarker);

/// Moves the given marker to the end of the text.
PRIVATE bool easygui_text_layout__move_marker_to_end_of_text(easygui_text_layout* pTL, easygui_text_marker* pMarker);

/// Moves the given marker to the start of the text.
PRIVATE bool easygui_text_layout__move_marker_to_start_of_text(easygui_text_layout* pTL, easygui_text_marker* pMarker);

/// Moves the given marker to the last character of the given run.
PRIVATE bool easygui_text_layout__move_marker_to_last_character_of_run(easygui_text_layout* pTL, easygui_text_marker* pMarker, unsigned int iRun);

/// Moves the given marker to the first character of the given run.
PRIVATE bool easygui_text_layout__move_marker_to_first_character_of_run(easygui_text_layout* pTL, easygui_text_marker* pMarker, unsigned int iRun);

/// Moves the given marker to the last character of the previous run.
PRIVATE bool easygui_text_layout__move_marker_to_last_character_of_prev_run(easygui_text_layout* pTL, easygui_text_marker* pMarker);

/// Moves the given marker to the first character of the next run.
PRIVATE bool easygui_text_layout__move_marker_to_first_character_of_next_run(easygui_text_layout* pTL, easygui_text_marker* pMarker);

/// Moves the given marker to the character at the given position.
PRIVATE bool easygui_text_layout__move_marker_to_character(easygui_text_layout* pTL, easygui_text_marker* pMarker, unsigned int iChar);


/// Updates the relative position of the given marker.
///
/// @remarks
///     This assumes the iRun and iChar properties are valid.
PRIVATE bool easygui_text_layout__update_marker_relative_position(easygui_text_layout* pTL, easygui_text_marker* pMarker);

/// Updates the sticky position of the given marker.
PRIVATE void easygui_text_layout__update_marker_sticky_position(easygui_text_layout* pTL, easygui_text_marker* pMarker);


/// Retrieves the index of the character the given marker is located at.
PRIVATE unsigned int easygui_text_layout__get_marker_absolute_char_index(easygui_text_layout* pTL, easygui_text_marker* pMarker);


/// Helper function for determining whether or not there is any spacing between the selection markers.
PRIVATE bool easygui_text_layout__has_spacing_between_selection_markers(easygui_text_layout* pTL);

/// Splits the given run into sub-runs based on the current selection rectangle. Returns the sub-run count.
PRIVATE unsigned int easygui_text_layout__split_text_run_by_selection(easygui_text_layout* pTL, easygui_text_run* pRunToSplit, easygui_text_run pSubRunsOut[3]);

/// Determines whether or not the run at the given index is selected.
PRIVATE bool easygui_text_layout__is_run_selected(easygui_text_layout* pTL, unsigned int iRun);

/// Retrieves pointers to the selection markers in the correct order.
PRIVATE bool easygui_text_layout__get_selection_markers(easygui_text_layout* pTL, easygui_text_marker** ppSelectionMarker0Out, easygui_text_marker** ppSelectionMarker1Out);


/// Retrieves an iterator to the first line in the text layout.
PRIVATE bool easygui_text_layout__first_line(easygui_text_layout* pTL, easygui_text_layout_line* pLine);

/// Retrieves an iterator to the next line in the text layout.
PRIVATE bool easygui_text_layout__next_line(easygui_text_layout* pTL, easygui_text_layout_line* pLine);


/// Removes the undo/redo state stack items after the current undo/redo point.
PRIVATE void easygui_text_layout__trim_undo_stack(easygui_text_layout* pTL);

/// Initializes the given undo state object by diff-ing the given layout states.
PRIVATE bool easygui_text_layout__diff_states(easygui_text_layout_state* pPrevState, easygui_text_layout_state* pCurrentState, easygui_text_layout_undo_state* pUndoStateOut);

/// Uninitializes the given undo state object. This basically just free's the internal string.
PRIVATE void easygui_text_layout__uninit_undo_state(easygui_text_layout_undo_state* pUndoState);

/// Pushes an undo state onto the undo stack.
PRIVATE void easygui_text_layout__push_undo_state(easygui_text_layout* pTL, easygui_text_layout_undo_state* pUndoState);
 
/// Applies the given undo state.
PRIVATE void easygui_text_layout__apply_undo_state(easygui_text_layout* pTL, easygui_text_layout_undo_state* pUndoState);

/// Applies the given undo state as a redo operation.
PRIVATE void easygui_text_layout__apply_redo_state(easygui_text_layout* pTL, easygui_text_layout_undo_state* pUndoState);


/// Retrieves a rectangle relative to the given text layout that's equal to the size of the container.
PRIVATE easygui_rect easygui_text_layout__local_rect(easygui_text_layout* pTL);



easygui_text_layout* easygui_create_text_layout(easygui_context* pContext, size_t extraDataSize, void* pExtraData)
{
    if (pContext == NULL) {
        return NULL;
    }

    easygui_text_layout* pTL = malloc(sizeof(easygui_text_layout) - sizeof(pTL->pExtraData) + extraDataSize);
    if (pTL == NULL) {
        return NULL;
    }

    pTL->text                       = NULL;
    pTL->textLength                 = 0;
    pTL->onDirty                    = NULL;
    pTL->containerWidth             = 0;
    pTL->containerHeight            = 0;
    pTL->innerOffsetX               = 0;
    pTL->innerOffsetY               = 0;
    pTL->pDefaultFont               = NULL;
    pTL->defaultTextColor           = easygui_rgb(224, 224, 224);
    pTL->defaultBackgroundColor     = easygui_rgb(48, 48, 48);
    pTL->selectionBackgroundColor   = easygui_rgb(64, 128, 192);
    pTL->tabSizeInSpaces            = 4;
    pTL->horzAlign                  = easygui_text_layout_alignment_left;
    pTL->vertAlign                  = easygui_text_layout_alignment_top;
    pTL->cursorWidth                = 1;
    pTL->cursorColor                = easygui_rgb(224, 224, 224);
    pTL->textBoundsWidth            = 0;
    pTL->textBoundsHeight           = 0;
    pTL->cursor                     = easygui_text_layout__new_marker();
    pTL->selectionAnchor            = easygui_text_layout__new_marker();
    pTL->selectionModeCounter       = 0;
    pTL->isAnythingSelected         = false;
    pTL->onPaintText                = NULL;
    pTL->onPaintRect                = NULL;
    pTL->onCursorMove               = NULL;
    pTL->preparedState.text         = NULL;
    pTL->pUndoStack                 = NULL;
    pTL->undoStackCount             = 0;
    pTL->iUndoState                 = 0;
    pTL->pRuns                      = NULL;
    pTL->runCount                   = 0;
    pTL->runBufferSize              = 0;

    pTL->extraDataSize = extraDataSize;
    if (pExtraData != NULL) {
        memcpy(pTL->pExtraData, pExtraData, extraDataSize);
    }

    return pTL;
}

void easygui_delete_text_layout(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return;
    }

    easygui_text_layout_clear_undo_stack(pTL);

    free(pTL->pRuns);
    free(pTL->preparedState.text);
    free(pTL->text);
    free(pTL);
}


size_t easygui_text_layout_get_extra_data_size(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return 0;
    }

    return pTL->extraDataSize;
}

void* easygui_text_layout_get_extra_data(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return NULL;
    }

    return pTL->pExtraData;
}


void easygui_text_layout_set_text(easygui_text_layout* pTL, const char* text)
{
    if (pTL == NULL) {
        return;
    }

    size_t textLength = strlen(text);

    free(pTL->text);
    pTL->text = malloc(textLength + 1);     // +1 for null terminator.

    // We now need to copy over the text, however we need to skip past \r characters in order to normalize line endings
    // and keep everything simple.
          char* dst = pTL->text;
    const char* src = text;
    while (*src != '\0')
    {
        if (*src != '\r') {
            *dst++ = *src;
        }

        src++;
    }
    *dst = '\0';

    pTL->textLength = dst - pTL->text;


    // A change in text means we need to refresh the layout.
    easygui_text_layout__refresh(pTL);

    if (pTL->onDirty) {
        pTL->onDirty(pTL, easygui_text_layout__local_rect(pTL));
    }
}

size_t easygui_text_layout_get_text(easygui_text_layout* pTL, char* textOut, size_t textOutSize)
{
    if (pTL == NULL) {
        return 0;
    }

    if (textOut == NULL) {
        return pTL->textLength;
    }


    if (strcpy_s(textOut, textOutSize, (pTL->text != NULL) ? pTL->text : "") == 0) {
        return pTL->textLength;
    }

    return 0;   // Error with strcpy_s().
}


void easygui_text_layout_set_on_dirty(easygui_text_layout* pTL, easygui_text_layout_on_dirty_proc proc)
{
    if (pTL == NULL) {
        return;
    }

    pTL->onDirty = proc;
}


void easygui_text_layout_set_container_size(easygui_text_layout* pTL, float containerWidth, float containerHeight)
{
    if (pTL == NULL) {
        return;
    }

    pTL->containerWidth  = containerWidth;
    pTL->containerHeight = containerHeight;

    if (pTL->onDirty) {
        pTL->onDirty(pTL, easygui_text_layout__local_rect(pTL));
    }
}

void easygui_text_layout_get_container_size(easygui_text_layout* pTL, float* pContainerWidthOut, float* pContainerHeightOut)
{
    float containerWidth  = 0;
    float containerHeight = 0;

    if (pTL != NULL)
    {
        containerWidth  = pTL->containerWidth;
        containerHeight = pTL->containerHeight;
    }


    if (pContainerWidthOut) {
        *pContainerWidthOut = containerWidth;
    }
    if (pContainerHeightOut) {
        *pContainerHeightOut = containerHeight;
    }
}

float easygui_text_layout_get_container_width(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return 0;
    }

    return pTL->containerWidth;
}

float easygui_text_layout_get_container_height(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return 0;
    }

    return pTL->containerHeight;
}


void easygui_text_layout_set_inner_offset(easygui_text_layout* pTL, float innerOffsetX, float innerOffsetY)
{
    if (pTL == NULL) {
        return;
    }

    pTL->innerOffsetX = innerOffsetX;
    pTL->innerOffsetY = innerOffsetY;

    if (pTL->onDirty) {
        pTL->onDirty(pTL, easygui_text_layout__local_rect(pTL));
    }
}

void easygui_text_layout_set_inner_offset_x(easygui_text_layout* pTL, float innerOffsetX)
{
    if (pTL == NULL) {
        return;
    }

    pTL->innerOffsetX = innerOffsetX;

    if (pTL->onDirty) {
        pTL->onDirty(pTL, easygui_text_layout__local_rect(pTL));
    }
}

void easygui_text_layout_set_inner_offset_y(easygui_text_layout* pTL, float innerOffsetY)
{
    if (pTL == NULL) {
        return;
    }

    pTL->innerOffsetY = innerOffsetY;

    if (pTL->onDirty) {
        pTL->onDirty(pTL, easygui_text_layout__local_rect(pTL));
    }
}

void easygui_text_layout_get_inner_offset(easygui_text_layout* pTL, float* pInnerOffsetX, float* pInnerOffsetY)
{
    float innerOffsetX = 0;
    float innerOffsetY = 0;

    if (pTL != NULL)
    {
        innerOffsetX = pTL->innerOffsetX;
        innerOffsetY = pTL->innerOffsetY;
    }


    if (pInnerOffsetX) {
        *pInnerOffsetX = innerOffsetX;
    }
    if (pInnerOffsetY) {
        *pInnerOffsetY = innerOffsetY;
    }
}

float easygui_text_layout_get_inner_offset_x(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return 0;
    }

    return pTL->innerOffsetX;
}

float easygui_text_layout_get_inner_offset_y(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return 0;
    }

    return pTL->innerOffsetY;
}


void easygui_text_layout_set_default_font(easygui_text_layout* pTL, easygui_font* pFont)
{
    if (pTL == NULL) {
        return;
    }

    pTL->pDefaultFont = pFont;

    // A change in font requires a layout refresh.
    easygui_text_layout__refresh(pTL);

    if (pTL->onDirty) {
        pTL->onDirty(pTL, easygui_text_layout__local_rect(pTL));
    }
}

easygui_font* easygui_text_layout_get_default_font(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return NULL;
    }

    return pTL->pDefaultFont;
}

void easygui_text_layout_set_default_text_color(easygui_text_layout* pTL, easygui_color color)
{
    if (pTL == NULL) {
        return;
    }

    pTL->defaultTextColor = color;

    if (pTL->onDirty) {
        pTL->onDirty(pTL, easygui_text_layout__local_rect(pTL));
    }
}

easygui_color easygui_text_layout_get_default_text_color(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return easygui_rgb(0, 0, 0);
    }

    return pTL->defaultTextColor;
}

void easygui_text_layout_set_default_bg_color(easygui_text_layout* pTL, easygui_color color)
{
    if (pTL == NULL) {
        return;
    }

    pTL->defaultBackgroundColor = color;

    if (pTL->onDirty) {
        pTL->onDirty(pTL, easygui_text_layout__local_rect(pTL));
    }
}

easygui_color easygui_text_layout_get_default_bg_color(easygui_text_layout* pTL)
{
    return pTL->defaultBackgroundColor;
}


void easygui_text_layout_set_tab_size(easygui_text_layout* pTL, unsigned int sizeInSpaces)
{
    if (pTL == NULL) {
        return;
    }

    if (pTL->tabSizeInSpaces != sizeInSpaces)
    {
        pTL->tabSizeInSpaces = sizeInSpaces;
        easygui_text_layout__refresh(pTL);

        if (pTL->onDirty) {
            pTL->onDirty(pTL, easygui_text_layout__local_rect(pTL));
        }
    }
}

unsigned int easygui_text_layout_get_tab_size(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return 0;
    }

    return pTL->tabSizeInSpaces;
}


void easygui_text_layout_set_horizontal_align(easygui_text_layout* pTL, easygui_text_layout_alignment alignment)
{
    if (pTL == NULL) {
        return;
    }

    if (pTL->horzAlign != alignment)
    {
        pTL->horzAlign = alignment;
        easygui_text_layout__refresh_alignment(pTL);

        if (pTL->onDirty) {
            pTL->onDirty(pTL, easygui_text_layout__local_rect(pTL));
        }
    }
}

easygui_text_layout_alignment easygui_text_layout_get_horizontal_align(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return easygui_text_layout_alignment_left;
    }

    return pTL->horzAlign;
}

void easygui_text_layout_set_vertical_align(easygui_text_layout* pTL, easygui_text_layout_alignment alignment)
{
    if (pTL == NULL) {
        return;
    }

    if (pTL->vertAlign != alignment)
    {
        pTL->vertAlign = alignment;
        easygui_text_layout__refresh_alignment(pTL);

        if (pTL->onDirty) {
            pTL->onDirty(pTL, easygui_text_layout__local_rect(pTL));
        }
    }
}

easygui_text_layout_alignment easygui_text_layout_get_vertical_align(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return easygui_text_layout_alignment_top;
    }

    return pTL->vertAlign;
}


easygui_rect easygui_text_layout_get_text_rect_relative_to_bounds(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return easygui_make_rect(0, 0, 0, 0);
    }

    easygui_rect rect;
    rect.left = 0;
    rect.top  = 0;


    switch (pTL->horzAlign)
    {
    case easygui_text_layout_alignment_right:
        {
            rect.left = pTL->containerWidth - pTL->textBoundsWidth;
            break;
        }

    case easygui_text_layout_alignment_center:
        {
            rect.left = (pTL->containerWidth - pTL->textBoundsWidth) / 2;
            break;
        }

    case easygui_text_layout_alignment_left:
    case easygui_text_layout_alignment_top:     // Invalid for horizontal align.
    case easygui_text_layout_alignment_bottom:  // Invalid for horizontal align.
    default:
        {
            break;
        }
    }


    switch (pTL->vertAlign)
    {
    case easygui_text_layout_alignment_bottom:
        {
            rect.top = pTL->containerHeight - pTL->textBoundsHeight;
            break;
        }

    case easygui_text_layout_alignment_center:
        {
            rect.top = (pTL->containerHeight - pTL->textBoundsHeight) / 2;
            break;
        }

    case easygui_text_layout_alignment_top:
    case easygui_text_layout_alignment_left:    // Invalid for vertical align.
    case easygui_text_layout_alignment_right:   // Invalid for vertical align.
    default:
        {
            break;
        }
    }


    rect.left  += pTL->innerOffsetX;
    rect.top   += pTL->innerOffsetY;
    rect.right  = rect.left + pTL->textBoundsWidth;
    rect.bottom = rect.top  + pTL->textBoundsHeight;

    return rect;
}


void easygui_text_layout_set_cursor_width(easygui_text_layout* pTL, float cursorWidth)
{
    if (pTL == NULL) {
        return;
    }

    easygui_rect oldCursorRect = easygui_text_layout_get_cursor_rect(pTL);
    pTL->cursorWidth = cursorWidth;

    if (pTL->onDirty) {
        pTL->onDirty(pTL, easygui_rect_union(oldCursorRect, easygui_text_layout_get_cursor_rect(pTL)));
    }
}

float easygui_text_layout_get_cursor_width(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return 0;
    }

    return pTL->cursorWidth;
}

void easygui_text_layout_set_cursor_color(easygui_text_layout* pTL, easygui_color cursorColor)
{
    if (pTL == NULL) {
        return;
    }

    pTL->cursorColor = cursorColor;

    if (pTL->onDirty) {
        pTL->onDirty(pTL, easygui_text_layout_get_cursor_rect(pTL));
    }
}

easygui_color easygui_text_layout_get_cursor_color(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return easygui_rgb(0, 0, 0);
    }

    return pTL->cursorColor;
}

void easygui_text_layout_move_cursor_to_point(easygui_text_layout* pTL, float posX, float posY)
{
    if (pTL == NULL) {
        return;
    }

    unsigned int iRunOld  = pTL->cursor.iRun;
    unsigned int iCharOld = pTL->cursor.iChar;
    easygui_text_layout__move_marker_to_point_relative_to_container(pTL, &pTL->cursor, posX, posY);

    if (iRunOld != pTL->cursor.iRun || iCharOld != pTL->cursor.iChar)
    {
        if (pTL->onCursorMove) {
            pTL->onCursorMove(pTL);
        }

        if (pTL->onDirty) {
            pTL->onDirty(pTL, easygui_text_layout__local_rect(pTL));
        }
    }


    if (easygui_text_layout_is_in_selection_mode(pTL)) {
        pTL->isAnythingSelected = easygui_text_layout__has_spacing_between_selection_markers(pTL);
    }
}

void easygui_text_layout_get_cursor_position(easygui_text_layout* pTL, float* pPosXOut, float* pPosYOut)
{
    if (pTL == NULL) {
        return;
    }

    easygui_text_layout__get_marker_position_relative_to_container(pTL, &pTL->cursor, pPosXOut, pPosYOut);
}

easygui_rect easygui_text_layout_get_cursor_rect(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return easygui_make_rect(0, 0, 0, 0);
    }

    easygui_rect lineRect = easygui_make_rect(0, 0, 0, 0);

    if (pTL->runCount > 0)
    {
        easygui_text_layout__find_line_info_by_index(pTL, pTL->pRuns[pTL->cursor.iRun].iLine, &lineRect, NULL, NULL);
    }
    else if (pTL->pDefaultFont != NULL)
    { 
        const float scaleX = 1;
        const float scaleY = 1;

        easygui_font_metrics defaultFontMetrics;
        easygui_get_font_metrics(pTL->pDefaultFont, scaleX, scaleY, &defaultFontMetrics);

        lineRect.bottom = (float)defaultFontMetrics.lineHeight;
    }
    


    float cursorPosX;
    float cursorPosY;
    easygui_text_layout_get_cursor_position(pTL, &cursorPosX, &cursorPosY);

    return easygui_make_rect(cursorPosX, cursorPosY, cursorPosX + pTL->cursorWidth, cursorPosY + (lineRect.bottom - lineRect.top));
}

unsigned int easygui_text_layout_get_cursor_line(easygui_text_layout* pTL)
{
    if (pTL == NULL || pTL->runCount == 0) {
        return 0;
    }

    return pTL->pRuns[pTL->cursor.iRun].iLine;
}

bool easygui_text_layout_move_cursor_left(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return false;
    }

    unsigned int iRunOld  = pTL->cursor.iRun;
    unsigned int iCharOld = pTL->cursor.iChar;
    if (easygui_text_layout__move_marker_left(pTL, &pTL->cursor)) {
        if (easygui_text_layout_is_in_selection_mode(pTL)) {
            pTL->isAnythingSelected = easygui_text_layout__has_spacing_between_selection_markers(pTL);
        }

        if (iRunOld != pTL->cursor.iRun || iCharOld != pTL->cursor.iChar) {
            if (pTL->onCursorMove) {
                pTL->onCursorMove(pTL);
            }

            if (pTL->onDirty) {
                pTL->onDirty(pTL, easygui_text_layout__local_rect(pTL));
            }
        }

        return true;
    }

    return false;
}

bool easygui_text_layout_move_cursor_right(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return false;
    }

    unsigned int iRunOld  = pTL->cursor.iRun;
    unsigned int iCharOld = pTL->cursor.iChar;
    if (easygui_text_layout__move_marker_right(pTL, &pTL->cursor)) {
        if (easygui_text_layout_is_in_selection_mode(pTL)) {
            pTL->isAnythingSelected = easygui_text_layout__has_spacing_between_selection_markers(pTL);
        }

        if (iRunOld != pTL->cursor.iRun || iCharOld != pTL->cursor.iChar) {
            if (pTL->onCursorMove) {
                pTL->onCursorMove(pTL);
            }

            if (pTL->onDirty) {
                pTL->onDirty(pTL, easygui_text_layout__local_rect(pTL));
            }
        }

        return true;
    }

    return false;
}

bool easygui_text_layout_move_cursor_up(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return false;
    }

    unsigned int iRunOld  = pTL->cursor.iRun;
    unsigned int iCharOld = pTL->cursor.iChar;
    if (easygui_text_layout__move_marker_up(pTL, &pTL->cursor)) {
        if (easygui_text_layout_is_in_selection_mode(pTL)) {
            pTL->isAnythingSelected = easygui_text_layout__has_spacing_between_selection_markers(pTL);
        }

        if (iRunOld != pTL->cursor.iRun || iCharOld != pTL->cursor.iChar) {
            if (pTL->onCursorMove) {
                pTL->onCursorMove(pTL);
            }

            if (pTL->onDirty) {
                pTL->onDirty(pTL, easygui_text_layout__local_rect(pTL));
            }
        }

        return true;
    }

    return false;
}

bool easygui_text_layout_move_cursor_down(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return false;
    }

    unsigned int iRunOld  = pTL->cursor.iRun;
    unsigned int iCharOld = pTL->cursor.iChar;
    if (easygui_text_layout__move_marker_down(pTL, &pTL->cursor)) {
        if (easygui_text_layout_is_in_selection_mode(pTL)) {
            pTL->isAnythingSelected = easygui_text_layout__has_spacing_between_selection_markers(pTL);
        }

        if (iRunOld != pTL->cursor.iRun || iCharOld != pTL->cursor.iChar) {
            if (pTL->onCursorMove) {
                pTL->onCursorMove(pTL);
            }

            if (pTL->onDirty) {
                pTL->onDirty(pTL, easygui_text_layout__local_rect(pTL));
            }
        }

        return true;
    }

    return false;
}

bool easygui_text_layout_move_cursor_to_end_of_line(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return false;
    }

    unsigned int iRunOld  = pTL->cursor.iRun;
    unsigned int iCharOld = pTL->cursor.iChar;
    if (easygui_text_layout__move_marker_to_end_of_line(pTL, &pTL->cursor)) {
        if (easygui_text_layout_is_in_selection_mode(pTL)) {
            pTL->isAnythingSelected = easygui_text_layout__has_spacing_between_selection_markers(pTL);
        }

        if (iRunOld != pTL->cursor.iRun || iCharOld != pTL->cursor.iChar) {
            if (pTL->onCursorMove) {
                pTL->onCursorMove(pTL);
            }

            if (pTL->onDirty) {
                pTL->onDirty(pTL, easygui_text_layout__local_rect(pTL));
            }
        }

        return true;
    }

    return false;
}

bool easygui_text_layout_move_cursor_to_start_of_line(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return false;
    }

    unsigned int iRunOld  = pTL->cursor.iRun;
    unsigned int iCharOld = pTL->cursor.iChar;
    if (easygui_text_layout__move_marker_to_start_of_line(pTL, &pTL->cursor)) {
        if (easygui_text_layout_is_in_selection_mode(pTL)) {
            pTL->isAnythingSelected = easygui_text_layout__has_spacing_between_selection_markers(pTL);
        }

        if (iRunOld != pTL->cursor.iRun || iCharOld != pTL->cursor.iChar) {
            if (pTL->onCursorMove) {
                pTL->onCursorMove(pTL);
            }

            if (pTL->onDirty) {
                pTL->onDirty(pTL, easygui_text_layout__local_rect(pTL));
            }
        }

        return true;
    }

    return false;
}

void easygui_text_layout_set_on_cursor_move(easygui_text_layout* pTL, easygui_text_layout_on_cursor_move_proc proc)
{
    if (pTL == NULL) {
        return;
    }

    pTL->onCursorMove = proc;
}


bool easygui_text_layout_insert_character(easygui_text_layout* pTL, unsigned int character, unsigned int insertIndex)
{
    if (pTL == NULL) {
        return false;
    }

    // Transform '\r' to '\n'.
    if (character == '\r') {
        character = '\n';
    }


    // TODO: Add proper support for UTF-8.
    char* pOldText = pTL->text;
    char* pNewText = malloc(pTL->textLength + 1 + 1);   // +1 for the new character and +1 for the null terminator.

    if (insertIndex > 0) {
        memcpy(pNewText, pOldText, insertIndex);
    }

    pNewText[insertIndex] = (char)character;

    if (insertIndex < pTL->textLength) {
        memcpy(pNewText + insertIndex + 1, pOldText + insertIndex, pTL->textLength - insertIndex);
    }

    pTL->textLength += 1;
    pTL->text = pNewText;
    pNewText[pTL->textLength] = '\0';

    free(pOldText);
    
    


    // The layout will have changed so it needs to be refreshed.
    easygui_text_layout__refresh(pTL);

    if (pTL->onDirty) {
        pTL->onDirty(pTL, easygui_text_layout__local_rect(pTL));
    }

    return true;
}

bool easygui_text_layout_insert_text(easygui_text_layout* pTL, const char* text, unsigned int insertIndex)
{
    if (pTL == NULL || text == NULL) {
        return false;;
    }

    size_t newTextLength = strlen(text);
    if (newTextLength == 0) {
        return false;
    }


    // TODO: Add proper support for UTF-8.
    char* pOldText = pTL->text;
    char* pNewText = malloc(pTL->textLength + newTextLength + 1);   // +1 for the new character and +1 for the null terminator.

    if (insertIndex > 0) {
        memcpy(pNewText, pOldText, insertIndex);
    }


    // Replace \r\n with \n.
    {
        char* dst = pNewText + insertIndex;
        const char* src = text;
        size_t srcLen = newTextLength;
        while (*src != '\0' && srcLen > 0)
        {
            if (*src != '\r') {
                *dst++ = *src;
            }

            src++;
            srcLen -= 1;
        }

        newTextLength = dst - (pNewText + insertIndex);
    }

    if (insertIndex < pTL->textLength) {
        memcpy(pNewText + insertIndex + newTextLength, pOldText + insertIndex, pTL->textLength - insertIndex);
    }

    pTL->textLength += newTextLength;
    pTL->text = pNewText;
    pNewText[pTL->textLength] = '\0';

    free(pOldText);


    // The layout will have changed so it needs to be refreshed.
    easygui_text_layout__refresh(pTL);

    if (pTL->onDirty) {
        pTL->onDirty(pTL, easygui_text_layout__local_rect(pTL));
    }

    return true;
}

bool easygui_text_layout_delete_text_range(easygui_text_layout* pTL, unsigned int iFirstCh, unsigned int iLastChPlus1)
{
    if (pTL == NULL || iLastChPlus1 == iFirstCh) {
        return false;
    }

    if (iFirstCh > iLastChPlus1) {
        unsigned int temp = iFirstCh;
        iFirstCh = iLastChPlus1;
        iLastChPlus1 = temp;
    }


    unsigned int bytesToRemove = iLastChPlus1 - iFirstCh;
    if (bytesToRemove > 0)
    {
        memmove(pTL->text + iFirstCh, pTL->text + iLastChPlus1, pTL->textLength - iLastChPlus1);
        pTL->textLength -= bytesToRemove;
        pTL->text[pTL->textLength] = '\0';

        // The layout will have changed.
        easygui_text_layout__refresh(pTL);

        if (pTL->onDirty) {
            pTL->onDirty(pTL, easygui_text_layout__local_rect(pTL));
        }

        return true;
    }

    return false;
}

bool easygui_text_layout_insert_character_at_cursor(easygui_text_layout* pTL, unsigned int character)
{
    if (pTL == NULL) {
        return false;
    }

    unsigned int iAbsoluteMarkerChar = 0;

    easygui_text_run* pRun = pTL->pRuns + pTL->cursor.iRun;
    if (pTL->runCount > 0 && pRun != NULL) {
        iAbsoluteMarkerChar = pRun->iChar + pTL->cursor.iChar;
    }
    

    easygui_text_layout_insert_character(pTL, character, iAbsoluteMarkerChar);


    // The marker needs to be updated based on the new layout and it's new position, which is one character ahead.
    easygui_text_layout__move_marker_to_character(pTL, &pTL->cursor, iAbsoluteMarkerChar + 1);

    // The cursor's sticky position needs to be updated whenever the text is edited.
    easygui_text_layout__update_marker_sticky_position(pTL, &pTL->cursor);


    if (pTL->onCursorMove) {
        pTL->onCursorMove(pTL);
    }

    return true;
}

bool easygui_text_layout_insert_text_at_cursor(easygui_text_layout* pTL, const char* text)
{
    if (pTL == NULL || text == NULL) {
        return false;
    }

    unsigned int cursorPos = easygui_text_layout__get_marker_absolute_char_index(pTL, &pTL->cursor);
    easygui_text_layout_insert_text(pTL, text, cursorPos);


    // The marker needs to be updated based on the new layout and it's new position, which is one character ahead.
    easygui_text_layout__move_marker_to_character(pTL, &pTL->cursor, cursorPos + strlen(text));

    // The cursor's sticky position needs to be updated whenever the text is edited.
    easygui_text_layout__update_marker_sticky_position(pTL, &pTL->cursor);


    if (pTL->onCursorMove) {
        pTL->onCursorMove(pTL);
    }

    return true;
}

bool easygui_text_layout_delete_character_to_left_of_cursor(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return false;;
    }

    // We just move the cursor to the left, and then delete the character to the right.
    if (easygui_text_layout_move_cursor_left(pTL)) {
        easygui_text_layout_delete_character_to_right_of_cursor(pTL);
        return true;
    }

    return false;
}

bool easygui_text_layout_delete_character_to_right_of_cursor(easygui_text_layout* pTL)
{
    if (pTL == NULL || pTL->runCount == 0) {
        return false;
    }

    easygui_text_run* pRun = pTL->pRuns + pTL->cursor.iRun;
    unsigned int iAbsoluteMarkerChar = pRun->iChar + pTL->cursor.iChar;

    if (iAbsoluteMarkerChar < pTL->textLength)
    {
        // TODO: Add proper support for UTF-8.
        memmove(pTL->text + iAbsoluteMarkerChar, pTL->text + iAbsoluteMarkerChar + 1, pTL->textLength - iAbsoluteMarkerChar);
        pTL->textLength -= 1;
        pTL->text[pTL->textLength] = '\0';



        // The layout will have changed.
        easygui_text_layout__refresh(pTL);

        if (pTL->onDirty) {
            pTL->onDirty(pTL, easygui_text_layout__local_rect(pTL));
        }

        return true;
    }

    return false;
}

bool easygui_text_layout_delete_selected_text(easygui_text_layout* pTL)
{
    // Don't do anything if nothing is selected.
    if (!easygui_text_layout_is_anything_selected(pTL)) {
        return false;
    }

    easygui_text_marker* pSelectionMarker0 = &pTL->selectionAnchor;
    easygui_text_marker* pSelectionMarker1 = &pTL->cursor;
    if (pTL->pRuns[pSelectionMarker0->iRun].iChar + pSelectionMarker0->iChar > pTL->pRuns[pSelectionMarker1->iRun].iChar + pSelectionMarker1->iChar)    
    {
        easygui_text_marker* temp = pSelectionMarker0;
        pSelectionMarker0 = pSelectionMarker1;
        pSelectionMarker1 = temp;
    }

    unsigned int iSelectionChar0 = pTL->pRuns[pSelectionMarker0->iRun].iChar + pSelectionMarker0->iChar;
    unsigned int iSelectionChar1 = pTL->pRuns[pSelectionMarker1->iRun].iChar + pSelectionMarker1->iChar;

    bool wasTextChanged = easygui_text_layout_delete_text_range(pTL, iSelectionChar0, iSelectionChar1);
    if (wasTextChanged)
    {
        // The marker needs to be updated based on the new layout.
        easygui_text_layout__move_marker_to_character(pTL, &pTL->cursor, iSelectionChar0);

        // The cursor's sticky position also needs to be updated.
        easygui_text_layout__update_marker_sticky_position(pTL, &pTL->cursor);

        if (pTL->onCursorMove) {
            pTL->onCursorMove(pTL);
        }


        // Reset the selection marker.
        pTL->selectionAnchor = pTL->cursor;
        pTL->isAnythingSelected = false;
    }

    return wasTextChanged;
}


void easygui_text_layout_enter_selection_mode(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return;
    }

    // If we've just entered selection mode and nothing is currently selected, we want to set the selection anchor to the current cursor position.
    if (!easygui_text_layout_is_in_selection_mode(pTL) && !pTL->isAnythingSelected) {
        pTL->selectionAnchor = pTL->cursor;
    }

    pTL->selectionModeCounter += 1;
}

void easygui_text_layout_leave_selection_mode(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return;
    }

    if (pTL->selectionModeCounter > 0) {
        pTL->selectionModeCounter -= 1;
    }
}

bool easygui_text_layout_is_in_selection_mode(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return false;
    }

    return pTL->selectionModeCounter > 0;
}

bool easygui_text_layout_is_anything_selected(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return false;
    }

    return pTL->isAnythingSelected;
}

void easygui_text_layout_deselect_all(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return;
    }

    pTL->isAnythingSelected = false;

    if (pTL->onDirty) {
        pTL->onDirty(pTL, easygui_text_layout__local_rect(pTL));
    }
}

void easygui_text_layout_select_all(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return;
    }

    easygui_text_layout__move_marker_to_start_of_text(pTL, &pTL->selectionAnchor);
    easygui_text_layout__move_marker_to_end_of_text(pTL, &pTL->cursor);

    pTL->isAnythingSelected = easygui_text_layout__has_spacing_between_selection_markers(pTL);

    if (pTL->onCursorMove) {
        pTL->onCursorMove(pTL);
    }

    if (pTL->onDirty) {
        pTL->onDirty(pTL, easygui_text_layout__local_rect(pTL));
    }
}

size_t easygui_text_layout_get_selected_text(easygui_text_layout* pTL, char* textOut, size_t textOutSize)
{
    if (pTL == NULL || (textOut != NULL && textOutSize == 0)) {
        return 0;
    }

    if (!easygui_text_layout_is_anything_selected(pTL)) {
        return 0;
    }


    easygui_text_marker* pSelectionMarker0;
    easygui_text_marker* pSelectionMarker1;
    if (!easygui_text_layout__get_selection_markers(pTL, &pSelectionMarker0, &pSelectionMarker1)) {
        return false;
    }

    unsigned int iSelectionChar0 = pTL->pRuns[pSelectionMarker0->iRun].iChar + pSelectionMarker0->iChar;
    unsigned int iSelectionChar1 = pTL->pRuns[pSelectionMarker1->iRun].iChar + pSelectionMarker1->iChar;

    size_t selectedTextLength = iSelectionChar1 - iSelectionChar0;

    if (textOut != NULL) {
        strncpy_s(textOut, textOutSize, pTL->text + iSelectionChar0, selectedTextLength);
    }
    
    return selectedTextLength;
}



bool easygui_text_layout_prepare_undo_point(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return false;
    }

    // If we have a previously prepared state we'll need to clear it.
    if (pTL->preparedState.text != NULL) {
        free(pTL->preparedState.text);
    }

    pTL->preparedState.text = malloc(pTL->textLength + 1);
    strcpy_s(pTL->preparedState.text, pTL->textLength + 1, (pTL->text != NULL) ? pTL->text : "");

    pTL->preparedState.cursorPos          = easygui_text_layout__get_marker_absolute_char_index(pTL, &pTL->cursor);
    pTL->preparedState.selectionAnchorPos = easygui_text_layout__get_marker_absolute_char_index(pTL, &pTL->selectionAnchor);
    pTL->preparedState.isAnythingSelected = pTL->isAnythingSelected;
    
    return true;
}

bool easygui_text_layout_commit_undo_point(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return false;
    }

    // The undo point must have been prepared earlier.
    if (pTL->preparedState.text == NULL) {
        return false;
    }


    // The undo state is creating by diff-ing the prepared state and the current state.
    easygui_text_layout_state currentState;
    currentState.text               = pTL->text;
    currentState.cursorPos          = easygui_text_layout__get_marker_absolute_char_index(pTL, &pTL->cursor);
    currentState.selectionAnchorPos = easygui_text_layout__get_marker_absolute_char_index(pTL, &pTL->selectionAnchor);
    currentState.isAnythingSelected = pTL->isAnythingSelected;

    easygui_text_layout_undo_state undoState;
    if (!easygui_text_layout__diff_states(&pTL->preparedState, &currentState, &undoState)) {
        return false;
    }


    // At this point we have the undo state ready and we just need to add it the undo stack. Before doing so, however,
    // we need to trim the end fo the stack.
    easygui_text_layout__trim_undo_stack(pTL);
    easygui_text_layout__push_undo_state(pTL, &undoState);

    return true;
}

bool easygui_text_layout_undo(easygui_text_layout* pTL)
{
    if (pTL == NULL || pTL->pUndoStack == NULL) {
        return false;
    }

    if (easygui_text_layout_get_undo_points_remaining_count(pTL) > 0)
    {
        easygui_text_layout_undo_state* pUndoState = pTL->pUndoStack + (pTL->iUndoState - 1);
        assert(pUndoState != NULL);

        easygui_text_layout__apply_undo_state(pTL, pUndoState);
        pTL->iUndoState -= 1;

        return true;
    }

    return false;
}

bool easygui_text_layout_redo(easygui_text_layout* pTL)
{
    if (pTL == NULL || pTL->pUndoStack == NULL) {
        return false;
    }

    if (easygui_text_layout_get_redo_points_remaining_count(pTL) > 0)
    {
        easygui_text_layout_undo_state* pUndoState = pTL->pUndoStack + pTL->iUndoState;
        assert(pUndoState != NULL);

        easygui_text_layout__apply_redo_state(pTL, pUndoState);
        pTL->iUndoState += 1;

        return true;
    }

    return false;
}

unsigned int easygui_text_layout_get_undo_points_remaining_count(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return 0;
    }

    return pTL->iUndoState;
}

unsigned int easygui_text_layout_get_redo_points_remaining_count(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return 0;
    }

    if (pTL->undoStackCount > 0)
    {
        assert(pTL->iUndoState <= pTL->undoStackCount);
        return pTL->undoStackCount - pTL->iUndoState;
    }

    return 0;
}

void easygui_text_layout_clear_undo_stack(easygui_text_layout* pTL)
{
    if (pTL == NULL || pTL->pUndoStack == NULL) {
        return;
    }
    
    for (unsigned int i = 0; i < pTL->undoStackCount; ++i) {
        easygui_text_layout__uninit_undo_state(pTL->pUndoStack + i);
    }

    free(pTL->pUndoStack);

    pTL->pUndoStack = NULL;
    pTL->undoStackCount = 0;
    pTL->iUndoState = 0;
}



unsigned int easygui_text_layout_get_line_count(easygui_text_layout* pTL)
{
    if (pTL == NULL || pTL->runCount == 0) {
        return 0;
    }

    return pTL->pRuns[pTL->runCount - 1].iLine + 1;
}

unsigned int easygui_text_layout_get_visible_line_count_starting_at(easygui_text_layout* pTL, unsigned int iFirstLine)
{
    if (pTL == NULL || pTL->runCount == 0) {
        return 0;
    }

    const float scaleX = 1;
    const float scaleY = 1;

    unsigned int count = 0;
    float lastLineBottom = 0;

    // First thing we do is find the first line.
    unsigned int iLine = 0;
    easygui_text_layout_line line;
    if (easygui_text_layout__first_line(pTL, &line))
    {
        do
        {
            if (iLine >= iFirstLine) {
                break;
            }

            iLine += 1;
        } while (easygui_text_layout__next_line(pTL, &line));


        // At this point we are at the first line and we need to start counting.
        do
        {
            if (line.posY + pTL->innerOffsetY >= pTL->containerHeight) {
                break;
            }

            count += 1;
            lastLineBottom = line.posY + line.height;

        } while (easygui_text_layout__next_line(pTL, &line));
    }

    
    // At this point there may be some empty space below the last line, in which case we use the line height of the default font to fill
    // out the remaining space.
    if (lastLineBottom + pTL->innerOffsetY < pTL->containerHeight)
    {
        easygui_font_metrics defaultFontMetrics;
        if (easygui_get_font_metrics(pTL->pDefaultFont, scaleX, scaleY, &defaultFontMetrics))
        {
            count += (unsigned int)((pTL->containerHeight - (lastLineBottom + pTL->innerOffsetY)) / defaultFontMetrics.lineHeight);
        }
    }


    
    if (count == 0) {
        return 1;
    }

    return count;
}

float easygui_text_layout_get_line_pos_y(easygui_text_layout* pTL, unsigned int iLine)
{
    easygui_rect lineRect;
    if (!easygui_text_layout__find_line_info_by_index(pTL, iLine, &lineRect, NULL, NULL)) {
        return 0;
    }

    return lineRect.top;
}


void easygui_text_layout_set_on_paint_text(easygui_text_layout* pTL, easygui_text_layout_on_paint_text_proc proc)
{
    if (pTL == NULL) {
        return;
    }

    pTL->onPaintText = proc;
}

void easygui_text_layout_set_on_paint_rect(easygui_text_layout* pTL, easygui_text_layout_on_paint_rect_proc proc)
{
    if (pTL == NULL) {
        return;
    }

    pTL->onPaintRect = proc;
}

void easygui_text_layout_paint(easygui_text_layout* pTL, easygui_rect rect, void* pUserData)
{
    if (pTL == NULL || pTL->onPaintText == NULL || pTL->onPaintRect == NULL) {
        return;
    }

    if (rect.left < 0) {
        rect.left = 0;
    }
    if (rect.top < 0) {
        rect.top = 0;
    }
    if (rect.right > pTL->containerWidth) {
        rect.right = pTL->containerWidth;
    }
    if (rect.bottom > pTL->containerHeight) {
        rect.bottom = pTL->containerHeight;
    }

    if (rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }


    const float scaleX = 1;
    const float scaleY = 1;


    // The position of each run will be relative to the text bounds. We want to make it relative to the container bounds.
    easygui_rect textRect = easygui_text_layout_get_text_rect_relative_to_bounds(pTL);

    // We draw a rectangle above and below the text rectangle. The main text rectangle will be drawn by iterating over each visible run.
    easygui_rect rectTop    = easygui_make_rect(0, 0, pTL->containerWidth, textRect.top);
    easygui_rect rectBottom = easygui_make_rect(0, textRect.bottom, pTL->containerWidth, pTL->containerHeight);

    if (pTL->onPaintRect)
    {
        if (rectTop.bottom > rect.top) {
            pTL->onPaintRect(pTL, rectTop, pTL->defaultBackgroundColor, pUserData);
        }
        
        if (rectBottom.top < rect.bottom) {
            pTL->onPaintRect(pTL, rectBottom, pTL->defaultBackgroundColor, pUserData);
        }
    }


    // We draw line-by-line, starting from the first visible line.
    easygui_text_layout_line line;
    if (easygui_text_layout__first_line(pTL, &line))
    {
        do
        {
            float lineTop    = line.posY + textRect.top;
            float lineBottom = lineTop + line.height;

            if (lineTop < rect.bottom)
            {
                if (lineBottom > rect.top)
                {
                    // The line is visible. We draw in 3 main parts - 1) the blank space to the left of the first run; 2) the runs themselves; 3) the blank
                    // space to the right of the last run.

                    float lineSelectionOverhangLeft  = 0;
                    float lineSelectionOverhangRight = 0;

                    if (easygui_text_layout_is_anything_selected(pTL))
                    {
                        easygui_text_marker* pSelectionMarker0 = &pTL->selectionAnchor;
                        easygui_text_marker* pSelectionMarker1 = &pTL->cursor;
                        if (pTL->pRuns[pSelectionMarker0->iRun].iChar + pSelectionMarker0->iChar > pTL->pRuns[pSelectionMarker1->iRun].iChar + pSelectionMarker1->iChar)    
                        {
                            easygui_text_marker* temp = pSelectionMarker0;
                            pSelectionMarker0 = pSelectionMarker1;
                            pSelectionMarker1 = temp;
                        }

                        unsigned int iSelectionLine0 = pTL->pRuns[pSelectionMarker0->iRun].iLine;
                        unsigned int iSelectionLine1 = pTL->pRuns[pSelectionMarker1->iRun].iLine;

                        if (line.index >= iSelectionLine0 && line.index < iSelectionLine1)
                        {
                            easygui_font_metrics defaultFontMetrics;
                            easygui_get_font_metrics(pTL->pDefaultFont, scaleX, scaleY, &defaultFontMetrics);

                            if (pTL->horzAlign == easygui_text_layout_alignment_right)
                            {
                                if (line.index > iSelectionLine0) {
                                    lineSelectionOverhangLeft = (float)defaultFontMetrics.spaceWidth;
                                }
                            }
                            else if (pTL->horzAlign == easygui_text_layout_alignment_center)
                            {
                                lineSelectionOverhangRight = (float)defaultFontMetrics.spaceWidth;

                                if (line.index > iSelectionLine0) {
                                    lineSelectionOverhangLeft = (float)defaultFontMetrics.spaceWidth;
                                }
                            }
                            else
                            {
                                lineSelectionOverhangRight = (float)defaultFontMetrics.spaceWidth;
                            }
                        }
                    }


                    easygui_text_run* pFirstRun = pTL->pRuns + line.iFirstRun;
                    easygui_text_run* pLastRun  = pTL->pRuns + line.iLastRun;

                    float lineLeft  = pFirstRun->posX + textRect.left;
                    float lineRight = pLastRun->posX + pLastRun->width + textRect.left;

                    // 1) The blank space to the left of the first run.
                    if (lineLeft > 0)
                    {
                        if (lineSelectionOverhangLeft > 0) {
                            pTL->onPaintRect(pTL, easygui_make_rect(lineLeft - lineSelectionOverhangLeft, lineTop, lineLeft, lineBottom), pTL->selectionBackgroundColor, pUserData);
                        }

                        pTL->onPaintRect(pTL, easygui_make_rect(0, lineTop, lineLeft - lineSelectionOverhangLeft, lineBottom), pTL->defaultBackgroundColor, pUserData);
                    }


                    // 2) The runs themselves.
                    for (unsigned int iRun = line.iFirstRun; iRun <= line.iLastRun; ++iRun)
                    {
                        easygui_text_run* pRun = pTL->pRuns + iRun;

                        float runLeft  = pRun->posX + textRect.left;
                        float runRight = runLeft    + pRun->width;

                        if (runRight > 0 && runLeft < pTL->containerWidth)
                        {
                            // The run is visible.
                            if (!easygui_text_layout__is_text_run_whitespace(pTL, pRun) || pTL->text[pRun->iChar] == '\t')
                            {
                                easygui_text_run run = pTL->pRuns[iRun];
                                run.pFont           = pTL->pDefaultFont;
                                run.textColor       = pTL->defaultTextColor;
                                run.backgroundColor = pTL->defaultBackgroundColor;
                                run.text            = pTL->text + run.iChar;
                                run.posX            = runLeft;
                                run.posY            = lineTop;

                                // We paint the run differently depending on whether or not anything is selected. If something is selected
                                // we need to split the run into a maximum of 3 sub-runs so that the selection rectangle can be drawn correctly.
                                if (easygui_text_layout_is_anything_selected(pTL))
                                {
                                    easygui_text_run subruns[3];
                                    unsigned int subrunCount = easygui_text_layout__split_text_run_by_selection(pTL, &run, subruns);
                                    for (unsigned int iSubRun = 0; iSubRun < subrunCount; ++iSubRun)
                                    {
                                        easygui_text_run* pSubRun = subruns + iSubRun;

                                        if (!easygui_text_layout__is_text_run_whitespace(pTL, pRun)) {
                                            pTL->onPaintText(pTL, pSubRun, pUserData);
                                        } else {
                                            pTL->onPaintRect(pTL, easygui_make_rect(pSubRun->posX, lineTop, pSubRun->posX + pSubRun->width, lineBottom), pSubRun->backgroundColor, pUserData);
                                        }
                                    }
                                }
                                else
                                {
                                    // Nothing is selected.
                                    if (!easygui_text_layout__is_text_run_whitespace(pTL, &run)) {
                                        pTL->onPaintText(pTL, &run, pUserData);
                                    } else {
                                        pTL->onPaintRect(pTL, easygui_make_rect(run.posX, lineTop, run.posX + run.width, lineBottom), run.backgroundColor, pUserData);
                                    }
                                }
                            }
                        }
                    }


                    // 3) The blank space to the right of the last run.
                    if (lineRight < pTL->containerWidth)
                    {
                        if (lineSelectionOverhangRight > 0) {
                            pTL->onPaintRect(pTL, easygui_make_rect(lineRight, lineTop, lineRight + lineSelectionOverhangRight, lineBottom), pTL->selectionBackgroundColor, pUserData);
                        }

                        pTL->onPaintRect(pTL, easygui_make_rect(lineRight + lineSelectionOverhangRight, lineTop, pTL->containerWidth, lineBottom), pTL->defaultBackgroundColor, pUserData);
                    }
                }
            }
            else
            {
                // The line is below the rectangle which means no other line will be visible and we can terminate early.
                break;
            }

        } while (easygui_text_layout__next_line(pTL, &line));
    }
}


PRIVATE bool easygui_next_run_string(const char* runStart, const char* textEndPastNullTerminator, const char** pRunEndOut)
{
    assert(runStart <= textEndPastNullTerminator);

    if (runStart == NULL || runStart == textEndPastNullTerminator)
    {
        // String is empty.
        return false;
    }


    char firstChar = runStart[0];
    if (firstChar == '\t')
    {
        // We loop until we hit anything that is not a tab character (tabs will be grouped together into a single run).
        do
        {
            runStart += 1;
            *pRunEndOut = runStart;
        } while (runStart[0] != '\0' && runStart[0] == '\t');
    }
    else if (firstChar == '\n')
    {
        runStart += 1;
        *pRunEndOut = runStart;
    }
    else if (firstChar == '\0')
    {
        assert(runStart + 1 == textEndPastNullTerminator);
        *pRunEndOut = textEndPastNullTerminator;
    }
    else
    {
        do
        {
            runStart += 1;
            *pRunEndOut = runStart;
        } while (runStart[0] != '\0' && runStart[0] != '\t' && runStart[0] != '\n');
    }

    return true;
}

PRIVATE void easygui_text_layout__refresh(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return;
    }

    // We split the runs based on tabs and new-lines. We want to create runs for tabs and new-line characters as well because we want
    // to have the entire string covered by runs for the sake of simplicity when it comes to editing.
    //
    // The first pass positions the runs based on a top-to-bottom, left-to-right alignment. The second pass then repositions the runs
    // based on alignment.

    // Runs need to be cleared first.
    easygui_text_layout__clear_text_runs(pTL);

    // The text bounds also need to be reset at the top.
    pTL->textBoundsWidth  = 0;
    pTL->textBoundsHeight = 0;

    const float scaleX = 1;
    const float scaleY = 1;

    easygui_font_metrics defaultFontMetrics;
    easygui_get_font_metrics(pTL->pDefaultFont, scaleX, scaleY, &defaultFontMetrics);

    const float tabWidth = easygui_text_layout__get_tab_width(pTL);

    unsigned int iCurrentLine  = 0;
    float runningPosY       = 0;
    float runningLineHeight = 0;

    const char* nextRunStart = pTL->text;
    const char* nextRunEnd;
    while (easygui_next_run_string(nextRunStart, pTL->text + pTL->textLength + 1, OUT &nextRunEnd))
    {
        easygui_text_run run;
        run.iLine          = iCurrentLine;
        run.iChar          = nextRunStart - pTL->text;
        run.iCharEnd       = nextRunEnd   - pTL->text;
        run.textLength     = nextRunEnd - nextRunStart;
        run.width          = 0;
        run.height         = 0;
        run.posX           = 0;
        run.posY           = runningPosY;
        run.pFont          = pTL->pDefaultFont;

        // X position
        //
        // The x position depends on the previous run that's on the same line.
        if (pTL->runCount > 0)
        {
            easygui_text_run* pPrevRun = pTL->pRuns + (pTL->runCount - 1);
            if (pPrevRun->iLine == iCurrentLine)
            {
                run.posX = pPrevRun->posX + pPrevRun->width;
            }
            else
            {
                // It's the first run on the line.
                run.posX = 0;
            }
        }


        // Width and height.
        assert(nextRunEnd > nextRunStart);
        if (nextRunStart[0] == '\t')
        {
            // Tab.
            const unsigned int tabCount = run.iCharEnd - run.iChar;
            run.width  = (float)(((tabCount*(unsigned int)tabWidth) - ((unsigned int)run.posX % (unsigned int)tabWidth)));
            run.height = (float)defaultFontMetrics.lineHeight;
        }
        else if (nextRunStart[0] == '\n')
        {
            // New line.
            iCurrentLine += 1;
            run.width  = 0;
            run.height = (float)defaultFontMetrics.lineHeight;
        }
        else if (nextRunStart[0] == '\0')
        {
            // Null terminator.
            run.width      = 0;
            run.height     = (float)defaultFontMetrics.lineHeight;
            run.textLength = 0;
        }
        else
        {
            // Normal run.
            easygui_measure_string(pTL->pDefaultFont, nextRunStart, run.textLength, 1, 1, &run.width, &run.height);
        }


        // The running line height needs to be updated by setting to the maximum size.
        runningLineHeight = (run.height > runningLineHeight) ? run.height : runningLineHeight;


        // Update the text bounds.
        if (pTL->textBoundsWidth < run.posX + run.width) {
            pTL->textBoundsWidth = run.posX + run.width;
        }
        pTL->textBoundsHeight = runningPosY + runningLineHeight;


        // A new line means we need to increment the running y position by the running line height.
        if (nextRunStart[0] == '\n')
        {
            runningPosY += runningLineHeight;
            runningLineHeight = 0;
        }

        // Add the run to the internal list.
        easygui_text_layout__push_text_run(pTL, &run);

        // Go to the next run string.
        nextRunStart = nextRunEnd;
    }

    // If we were to return now the text would be alignment top/left. If the alignment is not top/left we need to refresh the layout.
    if (pTL->horzAlign != easygui_text_layout_alignment_left || pTL->vertAlign != easygui_text_layout_alignment_top)
    {
        easygui_text_layout__refresh_alignment(pTL);
    }
}

PRIVATE void easygui_text_layout__refresh_alignment(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return;
    }

    float runningPosY = 0;

    unsigned int iCurrentLine = 0;
    for (size_t iRun = 0; iRun < pTL->runCount; DO_NOTHING)     // iRun is incremented from within the loop.
    {
        float lineWidth  = 0;
        float lineHeight = 0;

        // This loop does a few things. First, it defines the end point for the loop after this one (jRun). Second, it calculates
        // the line width which is needed for center and right alignment. Third it resets the position of each run to their
        // unaligned equivalents which will be offsetted in the second loop.
        size_t jRun;
        for (jRun = iRun; jRun < pTL->runCount && pTL->pRuns[jRun].iLine == iCurrentLine; ++jRun)
        {
            easygui_text_run* pRun = pTL->pRuns + jRun;
            pRun->posX = lineWidth;
            pRun->posY = runningPosY;

            lineWidth += pRun->width;
            lineHeight = (lineHeight > pRun->height) ? lineHeight : pRun->height;
        }

        
        // The actual alignment is done here.
        float lineOffsetX;
        float lineOffsetY;
        easygui_text_layout__calculate_line_alignment_offset(pTL, lineWidth, OUT &lineOffsetX, OUT &lineOffsetY);

        for (DO_NOTHING; iRun < jRun; ++iRun)
        {
            easygui_text_run* pRun = pTL->pRuns + iRun;
            pRun->posX += lineOffsetX;
            pRun->posY += lineOffsetY;
        }


        // Go to the next line.
        iCurrentLine += 1;
        runningPosY  += lineHeight;
    }
}

void easygui_text_layout__calculate_line_alignment_offset(easygui_text_layout* pTL, float lineWidth, float* pOffsetXOut, float* pOffsetYOut)
{
    if (pTL == NULL) {
        return;
    }

    float offsetX = 0;
    float offsetY = 0;

    switch (pTL->horzAlign)
    {
    case easygui_text_layout_alignment_right:
        {
            offsetX = pTL->textBoundsWidth - lineWidth;
            break;
        }

    case easygui_text_layout_alignment_center:
        {
            offsetX = (pTL->textBoundsWidth - lineWidth) / 2;
            break;
        }

    case easygui_text_layout_alignment_left:
    case easygui_text_layout_alignment_top:     // Invalid for horizontal alignment.
    case easygui_text_layout_alignment_bottom:  // Invalid for horizontal alignment.
    default:
        {
            offsetX = 0;
            break;
        }
    }


    switch (pTL->vertAlign)
    {
    case easygui_text_layout_alignment_bottom:
        {
            offsetY = pTL->textBoundsHeight - pTL->textBoundsHeight;
            break;
        }

    case easygui_text_layout_alignment_center:
        {
            offsetY = (pTL->textBoundsHeight - pTL->textBoundsHeight) / 2;
            break;
        }

    case easygui_text_layout_alignment_top:
    case easygui_text_layout_alignment_left:    // Invalid for vertical alignment.
    case easygui_text_layout_alignment_right:   // Invalid for vertical alignment.
    default:
        {
            offsetY = 0;
            break;
        }
    }


    if (pOffsetXOut) {
        *pOffsetXOut = offsetX;
    }
    if (pOffsetYOut) {
        *pOffsetYOut = offsetY;
    }
}


PRIVATE void easygui_text_layout__push_text_run(easygui_text_layout* pTL, easygui_text_run* pRun)
{
    if (pTL == NULL) {
        return;
    }

    if (pTL->runBufferSize == pTL->runCount)
    {
        pTL->runBufferSize = pTL->runBufferSize*2;
        if (pTL->runBufferSize == 0) {
            pTL->runBufferSize = 1;
        }

        pTL->pRuns = realloc(pTL->pRuns, sizeof(easygui_text_run) * pTL->runBufferSize);
        if (pTL->pRuns == NULL) {
            pTL->runCount = 0;
            pTL->runBufferSize = 0;
            return;
        }
    }

    pTL->pRuns[pTL->runCount] = *pRun;
    pTL->runCount += 1;
}

PRIVATE void easygui_text_layout__clear_text_runs(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return;
    }

    pTL->runCount = 0;
}

PRIVATE bool easygui_text_layout__is_text_run_whitespace(easygui_text_layout* pTL, easygui_text_run* pRun)
{
    if (pRun == NULL) {
        return false;
    }

    if (pTL->text[pRun->iChar] != '\t' && pTL->text[pRun->iChar] != '\n')
    {
        return false;
    }

    return true;
}

PRIVATE float easygui_text_layout__get_tab_width(easygui_text_layout* pTL)
{
    easygui_font_metrics defaultFontMetrics;
    easygui_get_font_metrics(pTL->pDefaultFont, 1, 1, &defaultFontMetrics);
                
    return (float)(defaultFontMetrics.spaceWidth * pTL->tabSizeInSpaces);
}


PRIVATE bool easygui_text_layout__find_closest_line_to_point(easygui_text_layout* pTL, float inputPosYRelativeToText, unsigned int* pFirstRunIndexOnLineOut, unsigned int* pLastRunIndexOnLinePlus1Out)
{
    if (pTL == NULL) {
        return false;
    }

    if (pTL->runCount > 0)
    {
        unsigned int iFirstRunOnLine     = 0;
        unsigned int iLastRunOnLinePlus1 = 0;

        float runningLineTop = 0;

        float lineHeight;
        while (easygui_text_layout__find_line_info(pTL, iFirstRunOnLine, OUT &iLastRunOnLinePlus1, OUT &lineHeight))
        {
            const float lineTop    = runningLineTop;
            const float lineBottom = lineTop + lineHeight;

            if (pFirstRunIndexOnLineOut) {
                *pFirstRunIndexOnLineOut = iFirstRunOnLine;
            }
            if (pLastRunIndexOnLinePlus1Out) {
                *pLastRunIndexOnLinePlus1Out = iLastRunOnLinePlus1;
            }

            if (inputPosYRelativeToText < lineBottom)
            {
                // It's on this line.
                break;
            }
            else
            {
                // It's not on this line - go to the next one.
                iFirstRunOnLine = iLastRunOnLinePlus1;
                runningLineTop  = lineBottom;
            }
        }

        return true;
    }

    return false;
}

PRIVATE bool easygui_text_layout__find_closest_run_to_point(easygui_text_layout* pTL, float inputPosXRelativeToText, float inputPosYRelativeToText, unsigned int* pRunIndexOut)
{
    if (pTL == NULL) {
        return false;
    }

    unsigned int iFirstRunOnLine;
    unsigned int iLastRunOnLinePlus1;
    if (easygui_text_layout__find_closest_line_to_point(pTL, inputPosYRelativeToText, OUT &iFirstRunOnLine, OUT &iLastRunOnLinePlus1))
    {
        unsigned int iRunOut = 0;

        const easygui_text_run* pFirstRunOnLine = pTL->pRuns + iFirstRunOnLine;
        const easygui_text_run* pLastRunOnLine  = pTL->pRuns + (iLastRunOnLinePlus1 - 1);

        if (inputPosXRelativeToText < pFirstRunOnLine->posX)
        {
            // It's to the left of the first run.
            iRunOut = iFirstRunOnLine;
        }
        else if (inputPosXRelativeToText > pLastRunOnLine->posX + pLastRunOnLine->width)
        {
            // It's to the right of the last run.
            iRunOut = iLastRunOnLinePlus1 - 1;
        }
        else
        {
            // It's in the middle of the line. We just iterate over each run on the line and return the first one that contains the point.
            for (unsigned int iRun = iFirstRunOnLine; iRun < iLastRunOnLinePlus1; ++iRun)
            {
                const easygui_text_run* pRun = pTL->pRuns + iRun;

                if (inputPosXRelativeToText >= pRun->posX && inputPosXRelativeToText <= pRun->posX + pRun->width)
                {
                    iRunOut = iRun;
                    break;
                }
            }
        }

        if (pRunIndexOut) {
            *pRunIndexOut = iRunOut;
        }

        return true;
    }
    else
    {
        // There was an error finding the closest line.
        return false;
    }
}

PRIVATE bool easygui_text_layout__find_line_info(easygui_text_layout* pTL, unsigned int iFirstRunOnLine, unsigned int* pLastRunIndexOnLinePlus1Out, float* pLineHeightOut)
{
    if (pTL == NULL) {
        return false;
    }

    if (iFirstRunOnLine < pTL->runCount)
    {
        const unsigned int iLine = pTL->pRuns[iFirstRunOnLine].iLine;
        float lineHeight = 0;

        unsigned int iRun;
        for (iRun = iFirstRunOnLine; iRun < pTL->runCount && pTL->pRuns[iRun].iLine == iLine; ++iRun)
        {
            if (lineHeight < pTL->pRuns[iRun].height) {
                lineHeight = pTL->pRuns[iRun].height;
            }
        }

        assert(iRun > iFirstRunOnLine);

        if (pLastRunIndexOnLinePlus1Out) {
            *pLastRunIndexOnLinePlus1Out = iRun;
        }
        if (pLineHeightOut) {
            *pLineHeightOut = lineHeight;
        }

        return true;
    }

    return false;
}

PRIVATE bool easygui_text_layout__find_line_info_by_index(easygui_text_layout* pTL, unsigned int iLine, easygui_rect* pRectOut, unsigned int* pFirstRunIndexOut, unsigned int* pLastRunIndexPlus1Out)
{
    if (pTL == NULL) {
        return false;
    }

    unsigned int iFirstRunOnLine     = 0;
    unsigned int iLastRunOnLinePlus1 = 0;

    float lineTop    = 0;
    float lineHeight = 0;

    for (unsigned int iCurrentLine = 0; iCurrentLine <= iLine; ++iCurrentLine)
    {
        lineTop += lineHeight;

        if (!easygui_text_layout__find_line_info(pTL, iFirstRunOnLine, &iLastRunOnLinePlus1, &lineHeight))
        {
            // There was an error retrieving information about the line.
            return false;
        }
    }


    // At this point we have the first and last runs that make up the line and we can generate our output.
    if (iLastRunOnLinePlus1 > iFirstRunOnLine)
    {
        if (pFirstRunIndexOut) {
            *pFirstRunIndexOut = iFirstRunOnLine;
        }
        if (pLastRunIndexPlus1Out) {
            *pLastRunIndexPlus1Out = iLastRunOnLinePlus1;
        }

        if (pRectOut != NULL)
        {
            pRectOut->left   = pTL->pRuns[iFirstRunOnLine].posX;
            pRectOut->right  = pTL->pRuns[iLastRunOnLinePlus1 - 1].posX + pTL->pRuns[iLastRunOnLinePlus1 - 1].width;
            pRectOut->top    = lineTop;
            pRectOut->bottom = pRectOut->top + lineHeight;
        }

        return true;
    }
    else
    {
        // We couldn't find any runs.
        return false;
    }
}

PRIVATE bool easygui_text_layout__find_last_run_on_line_starting_from_run(easygui_text_layout* pTL, unsigned int iRun, unsigned int* pLastRunIndexOnLineOut)
{
    if (pTL == NULL) {
        return false;
    }

    unsigned int result = iRun;

    unsigned int iLine = pTL->pRuns[iRun].iLine;
    for (DO_NOTHING; iRun < pTL->runCount && pTL->pRuns[iRun].iLine == iLine; ++iRun)
    {
        result = iRun;
    }

    if (pLastRunIndexOnLineOut) {
        *pLastRunIndexOnLineOut = result;
    }

    return true;
}

PRIVATE bool easygui_text_layout__find_first_run_on_line_starting_from_run(easygui_text_layout* pTL, unsigned int iRun, unsigned int* pFirstRunIndexOnLineOut)
{
    if (pTL == NULL) {
        return false;
    }

    unsigned int result = iRun;

    unsigned int iLine = pTL->pRuns[iRun].iLine;
    for (DO_NOTHING; iRun > 0 && pTL->pRuns[iRun - 1].iLine == iLine; --iRun)
    {
        result = iRun - 1;
    }

    if (pFirstRunIndexOnLineOut) {
        *pFirstRunIndexOnLineOut = result;
    }

    return true;
}

PRIVATE bool easygui_text_layout__find_run_at_character(easygui_text_layout* pTL, unsigned int iChar, unsigned int* pRunIndexOut)
{
    if (pTL == NULL || pTL->runCount == 0) {
        return false;
    }

    unsigned int result = 0;
    if (iChar < pTL->textLength)
    {
        for (unsigned int iRun = 0; iRun < pTL->runCount; ++iRun)
        {
            const easygui_text_run* pRun = pTL->pRuns + iRun;

            if (iChar < pRun->iCharEnd)
            {
                result = iRun;
                break;
            }
        }
    }
    else
    {
        // The character index is too high. Return the last run.
        result = pTL->runCount - 1;
    }

    if (pRunIndexOut) {
        *pRunIndexOut = result;
    }

    return true;
}


PRIVATE easygui_text_marker easygui_text_layout__new_marker()
{
    easygui_text_marker marker;
    marker.iRun              = 0;
    marker.iChar             = 0;
    marker.relativePosX      = 0;
    marker.absoluteSickyPosX = 0;

    return marker;
}

PRIVATE bool easygui_text_layout__move_marker_to_point_relative_to_container(easygui_text_layout* pTL, easygui_text_marker* pMarker, float inputPosX, float inputPosY)
{
    if (pTL == NULL || pMarker == NULL) {
        return false;
    }

    pMarker->iRun              = 0;
    pMarker->iChar             = 0;
    pMarker->relativePosX      = 0;
    pMarker->absoluteSickyPosX = 0;

    easygui_rect textRect = easygui_text_layout_get_text_rect_relative_to_bounds(pTL);
    
    float inputPosXRelativeToText = inputPosX - textRect.left;
    float inputPosYRelativeToText = inputPosY - textRect.top;
    if (easygui_text_layout__move_marker_to_point(pTL, pMarker, inputPosXRelativeToText, inputPosYRelativeToText))
    {
        easygui_text_layout__update_marker_sticky_position(pTL, pMarker);
        return true;
    }

    return false;
}

PRIVATE void easygui_text_layout__get_marker_position_relative_to_container(easygui_text_layout* pTL, easygui_text_marker* pMarker, float* pPosXOut, float* pPosYOut)
{
    if (pTL == NULL || pMarker == NULL) {
        return;
    }

    float posX = 0;
    float posY = 0;

    if (pMarker->iRun < pTL->runCount)
    {
        easygui_rect textRect = easygui_text_layout_get_text_rect_relative_to_bounds(pTL);

        posX = textRect.left + pTL->pRuns[pMarker->iRun].posX + pMarker->relativePosX;
        posY = textRect.top  + pTL->pRuns[pMarker->iRun].posY;
    }


    if (pPosXOut) {
        *pPosXOut = posX;
    }
    if (pPosYOut) {
        *pPosYOut = posY;
    }
}

PRIVATE bool easygui_text_layout__move_marker_to_point(easygui_text_layout* pTL, easygui_text_marker* pMarker, float inputPosXRelativeToText, float inputPosYRelativeToText)
{
    if (pTL == NULL || pMarker == NULL) {
        return false;
    }

    const float scaleX = 1;
    const float scaleY = 1;

    unsigned int iClosestRunToPoint;
    if (easygui_text_layout__find_closest_run_to_point(pTL, inputPosXRelativeToText, inputPosYRelativeToText, OUT &iClosestRunToPoint))
    {
        const easygui_text_run* pRun = pTL->pRuns + iClosestRunToPoint;

        pMarker->iRun = iClosestRunToPoint;

        if (inputPosXRelativeToText < pRun->posX)
        {
            // It's to the left of the run.
            pMarker->iChar        = 0;
            pMarker->relativePosX = 0;
        }
        else if (inputPosXRelativeToText > pRun->posX + pRun->width)
        {
            // It's to the right of the run. It may be a new-line run. If so, we need to move the marker to the front of it, not the back.
            pMarker->iChar        = pRun->textLength;
            pMarker->relativePosX = pRun->width;

            if (pTL->text[pRun->iChar] == '\n') {
                assert(pMarker->iChar == 1);
                pMarker->iChar        = 0;
                pMarker->relativePosX = 0;
            }
        }
        else
        {
            // It's somewhere in the middle of the run. We need to handle this a little different for tab runs since they are aligned differently.
            if (pTL->text[pRun->iChar] == '\n')
            {
                // It's a new line character. It needs to be placed at the beginning of it.
                pMarker->iChar        = 0;
                pMarker->relativePosX = 0;
            }
            else if (pTL->text[pRun->iChar] == '\t')
            {
                // It's a tab run.
                pMarker->iChar        = 0;
                pMarker->relativePosX = 0;

                const float tabWidth = easygui_text_layout__get_tab_width(pTL);

                float tabLeft = pRun->posX + pMarker->relativePosX;
                for (DO_NOTHING; pMarker->iChar < pRun->textLength; ++pMarker->iChar)
                {
                    float tabRight = tabWidth * ((pRun->posX + (tabWidth*(pMarker->iChar + 1))) / tabWidth);
                    if (tabRight > pRun->posX + pRun->width) {
                        tabRight = pRun->posX + pRun->width;
                    }

                    if (inputPosXRelativeToText >= tabLeft && inputPosXRelativeToText <= tabRight)
                    {
                        // The input position is somewhere on top of this character. If it's positioned on the left side of the character, set the output
                        // value to the character at iChar. Otherwise it should be set to the character at iChar + 1.
                        float charBoundsRightHalf = tabLeft + ceilf(((tabRight - tabLeft) / 2.0f));
                        if (inputPosXRelativeToText <= charBoundsRightHalf) {
                            pMarker->relativePosX = tabLeft - pRun->posX;
                        } else {
                            pMarker->relativePosX = tabRight - pRun->posX;
                            pMarker->iChar += 1;
                        }

                        break;
                    }

                    tabLeft = tabRight;
                }

                // If we're past the last character in the tab run, we want to move to the start of the next run.
                if (pMarker->iChar == pRun->textLength) {
                    easygui_text_layout__move_marker_to_first_character_of_next_run(pTL, pMarker);
                }
            }
            else
            {
                // It's a standard run.
                float inputPosXRelativeToRun = inputPosXRelativeToText - pRun->posX;
                if (easygui_get_text_cursor_position_from_point(pRun->pFont, pTL->text + pRun->iChar, pRun->textLength, pRun->width, inputPosXRelativeToRun, scaleX, scaleY, OUT &pMarker->relativePosX, OUT &pMarker->iChar))
                {
                    // If the marker is past the last character of the run it needs to be moved to the start of the next one.
                    if (pMarker->iChar == pRun->textLength) {
                        easygui_text_layout__move_marker_to_first_character_of_next_run(pTL, pMarker);
                    }
                }
                else
                {
                    // An error occured somehow.
                    return false;
                }
            }
        }

        return true;
    }
    else
    {
        // Couldn't find a run.
        return false;
    }

    return true;
}

PRIVATE bool easygui_text_layout__move_marker_left(easygui_text_layout* pTL, easygui_text_marker* pMarker)
{
    if (pTL == NULL || pMarker == NULL) {
        return false;
    }

    const float scaleX = 1;
    const float scaleY = 1;

    if (pTL->runCount > 0)
    {
        if (pMarker->iChar > 0)
        {
            pMarker->iChar -= 1;

            const easygui_text_run* pRun = pTL->pRuns + pMarker->iRun;
            if (pTL->text[pRun->iChar] == '\t')
            {
                const float tabWidth = easygui_text_layout__get_tab_width(pTL);

                if (pMarker->iChar == 0)
                {
                    // Simple case - it's the first tab character which means the relative position is just 0.
                    pMarker->relativePosX = 0;
                }
                else
                {
                    pMarker->relativePosX  = tabWidth * ((pRun->posX + (tabWidth*(pMarker->iChar + 0))) / tabWidth);
                    pMarker->relativePosX -= pRun->posX;
                }
            }
            else
            {
                if (!easygui_get_text_cursor_position_from_char(pRun->pFont, pTL->text + pTL->pRuns[pMarker->iRun].iChar, pMarker->iChar, scaleX, scaleY, OUT &pMarker->relativePosX))
                {
                    return false;
                }
            }
        }
        else
        {
            // We're at the beginning of the run which means we need to transfer the cursor to the end of the previous run.
            if (!easygui_text_layout__move_marker_to_last_character_of_prev_run(pTL, pMarker))
            {
                return false;
            }
        }

        easygui_text_layout__update_marker_sticky_position(pTL, pMarker);
        return true;
    }

    return false;
}

PRIVATE bool easygui_text_layout__move_marker_right(easygui_text_layout* pTL, easygui_text_marker* pMarker)
{
    if (pTL == NULL || pMarker == NULL) {
        return false;
    }

    const float scaleX = 1;
    const float scaleY = 1;

    if (pTL->runCount > 0)
    {
        if (pMarker->iChar + 1 < pTL->pRuns[pMarker->iRun].textLength)
        {
            pMarker->iChar += 1;

            const easygui_text_run* pRun = pTL->pRuns + pMarker->iRun;
            if (pTL->text[pRun->iChar] == '\t')
            {
                const float tabWidth = easygui_text_layout__get_tab_width(pTL);

                pMarker->relativePosX  = tabWidth * ((pRun->posX + (tabWidth*(pMarker->iChar + 0))) / tabWidth);
                pMarker->relativePosX -= pRun->posX;
            }
            else
            {
                if (!easygui_get_text_cursor_position_from_char(pRun->pFont, pTL->text + pTL->pRuns[pMarker->iRun].iChar, pMarker->iChar, scaleX, scaleY, OUT &pMarker->relativePosX))
                {
                    return false;
                }
            }
        }
        else
        {
            // We're at the end of the run which means we need to transfer the cursor to the beginning of the next run.
            if (!easygui_text_layout__move_marker_to_first_character_of_next_run(pTL, pMarker))
            {
                return false;
            }
        }

        easygui_text_layout__update_marker_sticky_position(pTL, pMarker);
        return true;
    }

    return false;
}

PRIVATE bool easygui_text_layout__move_marker_up(easygui_text_layout* pTL, easygui_text_marker* pMarker)
{
    if (pTL == NULL || pMarker == NULL || pTL->runCount == 0) {
        return false;
    }

    const easygui_text_run* pOldRun = pTL->pRuns + pMarker->iRun;
    if (pOldRun->iLine > 0)
    {
        easygui_rect lineRect;
        unsigned int iFirstRunOnLine;
        unsigned int iLastRunOnLinePlus1;
        if (easygui_text_layout__find_line_info_by_index(pTL, pOldRun->iLine - 1, OUT &lineRect, OUT &iFirstRunOnLine, OUT &iLastRunOnLinePlus1))
        {
            float newMarkerPosX = pMarker->absoluteSickyPosX;
            float newMarkerPosY = lineRect.top;
            easygui_text_layout__move_marker_to_point(pTL, pMarker, newMarkerPosX, newMarkerPosY);

            return true;
        }
        else
        {
            // An error occured while finding information about the line above.
            return false;
        }
    }
    else
    {
        // The cursor is already on the top line.
        return false;
    }
}

PRIVATE bool easygui_text_layout__move_marker_down(easygui_text_layout* pTL, easygui_text_marker* pMarker)
{
    if (pTL == NULL || pMarker == NULL || pTL->runCount == 0) {
        return false;
    }

    const easygui_text_run* pOldRun = pTL->pRuns + pMarker->iRun;

    easygui_rect lineRect;
    unsigned int iFirstRunOnLine;
    unsigned int iLastRunOnLinePlus1;
    if (easygui_text_layout__find_line_info_by_index(pTL, pOldRun->iLine + 1, OUT &lineRect, OUT &iFirstRunOnLine, OUT &iLastRunOnLinePlus1))
    {
        float newMarkerPosX = pMarker->absoluteSickyPosX;
        float newMarkerPosY = lineRect.top;
        easygui_text_layout__move_marker_to_point(pTL, pMarker, newMarkerPosX, newMarkerPosY);

        return true;
    }
    else
    {
        // An error occured while finding information about the line above.
        return false;
    }
}

PRIVATE bool easygui_text_layout__move_marker_to_end_of_line(easygui_text_layout* pTL, easygui_text_marker* pMarker)
{
    if (pTL == NULL || pMarker == NULL) {
        return false;
    }

    unsigned int iLastRunOnLine;
    if (easygui_text_layout__find_last_run_on_line_starting_from_run(pTL, pMarker->iRun, &iLastRunOnLine))
    {
        return easygui_text_layout__move_marker_to_last_character_of_run(pTL, pMarker, iLastRunOnLine);
    }

    return false;
}

PRIVATE bool easygui_text_layout__move_marker_to_start_of_line(easygui_text_layout* pTL, easygui_text_marker* pMarker)
{
    if (pTL == NULL || pMarker == NULL) {
        return false;
    }

    unsigned int iFirstRunOnLine;
    if (easygui_text_layout__find_first_run_on_line_starting_from_run(pTL, pMarker->iRun, &iFirstRunOnLine))
    {
        return easygui_text_layout__move_marker_to_first_character_of_run(pTL, pMarker, iFirstRunOnLine);
    }

    return false;
}

PRIVATE bool easygui_text_layout__move_marker_to_end_of_text(easygui_text_layout* pTL, easygui_text_marker* pMarker)
{
    if (pTL == NULL || pMarker == NULL) {
        return false;
    }

    if (pTL->runCount > 0) {
        return easygui_text_layout__move_marker_to_last_character_of_run(pTL, pMarker, pTL->runCount - 1);
    }

    return false;
}

PRIVATE bool easygui_text_layout__move_marker_to_start_of_text(easygui_text_layout* pTL, easygui_text_marker* pMarker)
{
    if (pTL == NULL || pMarker == NULL) {
        return false;
    }

    return easygui_text_layout__move_marker_to_first_character_of_run(pTL, pMarker, 0);
}

PRIVATE bool easygui_text_layout__move_marker_to_last_character_of_run(easygui_text_layout* pTL, easygui_text_marker* pMarker, unsigned int iRun)
{
    if (pTL == NULL || pMarker == NULL) {
        return false;
    }

    if (iRun < pTL->runCount)
    {
        pMarker->iRun         = iRun;
        pMarker->iChar        = pTL->pRuns[pMarker->iRun].textLength;
        pMarker->relativePosX = pTL->pRuns[pMarker->iRun].width;

        if (pMarker->iChar > 0)
        {
            // At this point we are located one character past the last character - we need to move it left.
            return easygui_text_layout__move_marker_left(pTL, pMarker);
        }

        return true;
    }

    return false;
}

PRIVATE bool easygui_text_layout__move_marker_to_first_character_of_run(easygui_text_layout* pTL, easygui_text_marker* pMarker, unsigned int iRun)
{
    if (pTL == NULL || pMarker == NULL) {
        return false;
    }

    if (iRun < pTL->runCount)
    {
        pMarker->iRun         = iRun;
        pMarker->iChar        = 0;
        pMarker->relativePosX = 0;

        return true;
    }

    return false;
}

PRIVATE bool easygui_text_layout__move_marker_to_last_character_of_prev_run(easygui_text_layout* pTL, easygui_text_marker* pMarker)
{
    if (pTL == NULL || pMarker == NULL) {
        return false;
    }

    if (pMarker->iRun > 0) {
        return easygui_text_layout__move_marker_to_last_character_of_run(pTL, pMarker, pMarker->iRun - 1);
    }

    return false;
}

PRIVATE bool easygui_text_layout__move_marker_to_first_character_of_next_run(easygui_text_layout* pTL, easygui_text_marker* pMarker)
{
    if (pTL == NULL || pMarker == NULL) {
        return false;
    }

    if (pTL->runCount > 0 && pMarker->iRun < pTL->runCount - 1) {
        return easygui_text_layout__move_marker_to_first_character_of_run(pTL, pMarker, pMarker->iRun + 1);
    }

    return false;
}

PRIVATE bool easygui_text_layout__move_marker_to_character(easygui_text_layout* pTL, easygui_text_marker* pMarker, unsigned int iChar)
{
    if (pTL == NULL || pMarker == NULL || pTL->runCount == 0) {
        return false;
    }

    // Clamp the character to the end of the string.
    if (iChar > pTL->textLength) {
        iChar = pTL->textLength;
    }

    easygui_text_layout__find_run_at_character(pTL, iChar, &pMarker->iRun);
        
    assert(pMarker->iRun < pTL->runCount);
    pMarker->iChar = iChar - pTL->pRuns[pMarker->iRun].iChar;


    // The relative position depends on whether or not the run is a tab character.
    return easygui_text_layout__update_marker_relative_position(pTL, pMarker);
}


PRIVATE bool easygui_text_layout__update_marker_relative_position(easygui_text_layout* pTL, easygui_text_marker* pMarker)
{
    if (pTL == NULL || pMarker == NULL || pTL->runCount == 0) {
        return false;
    }

    float scaleX = 1;
    float scaleY = 1;

    const easygui_text_run* pRun = pTL->pRuns + pMarker->iRun;
    if (pTL->text[pRun->iChar] == '\t')
    {
        const float tabWidth = easygui_text_layout__get_tab_width(pTL);

        if (pMarker->iChar == 0)
        {
            // Simple case - it's the first tab character which means the relative position is just 0.
            pMarker->relativePosX = 0;
        }
        else
        {
            pMarker->relativePosX  = tabWidth * ((pRun->posX + (tabWidth*(pMarker->iChar + 0))) / tabWidth);
            pMarker->relativePosX -= pRun->posX;
        }

        return true;
    }
    else
    {
        return easygui_get_text_cursor_position_from_char(pRun->pFont, pTL->text + pTL->pRuns[pMarker->iRun].iChar, pMarker->iChar, scaleX, scaleY, OUT &pMarker->relativePosX);
    }
}

PRIVATE void easygui_text_layout__update_marker_sticky_position(easygui_text_layout* pTL, easygui_text_marker* pMarker)
{
    if (pTL == NULL || pMarker == NULL) {
        return;
    }

    pMarker->absoluteSickyPosX = pTL->pRuns[pMarker->iRun].posX + pMarker->relativePosX;
}

PRIVATE unsigned int easygui_text_layout__get_marker_absolute_char_index(easygui_text_layout* pTL, easygui_text_marker* pMarker)
{
    if (pTL == NULL || pMarker == NULL || pTL->runCount == 0) {
        return 0;
    }

    return pTL->pRuns[pMarker->iRun].iChar + pMarker->iChar;
}


PRIVATE bool easygui_text_layout__has_spacing_between_selection_markers(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return false;
    }

    return (pTL->cursor.iRun != pTL->selectionAnchor.iRun || pTL->cursor.iChar != pTL->selectionAnchor.iChar);
}

PRIVATE unsigned int easygui_text_layout__split_text_run_by_selection(easygui_text_layout* pTL, easygui_text_run* pRunToSplit, easygui_text_run pSubRunsOut[3])
{
    if (pTL == NULL || pRunToSplit == NULL || pSubRunsOut == NULL) {
        return 0;
    }

    easygui_text_marker* pSelectionMarker0 = &pTL->selectionAnchor;
    easygui_text_marker* pSelectionMarker1 = &pTL->cursor;
    if (pTL->pRuns[pSelectionMarker0->iRun].iChar + pSelectionMarker0->iChar > pTL->pRuns[pSelectionMarker1->iRun].iChar + pSelectionMarker1->iChar)    
    {
        easygui_text_marker* temp = pSelectionMarker0;
        pSelectionMarker0 = pSelectionMarker1;
        pSelectionMarker1 = temp;
    }

    easygui_text_run* pSelectionRun0 = pTL->pRuns + pSelectionMarker0->iRun;
    easygui_text_run* pSelectionRun1 = pTL->pRuns + pSelectionMarker1->iRun;

    unsigned int iSelectionChar0 = pSelectionRun0->iChar + pSelectionMarker0->iChar;
    unsigned int iSelectionChar1 = pSelectionRun1->iChar + pSelectionMarker1->iChar;

    if (easygui_text_layout_is_anything_selected(pTL))
    {
        if (pRunToSplit->iChar < iSelectionChar1 && pRunToSplit->iCharEnd > iSelectionChar0)
        {
            // The run is somewhere inside the selection region.
            for (int i = 0; i < 3; ++i) {
                pSubRunsOut[i] = *pRunToSplit;
            }

            if (pRunToSplit->iChar >= iSelectionChar0)
            {
                // The first part of the run is selected.
                if (pRunToSplit->iCharEnd <= iSelectionChar1)
                {
                    // The entire run is selected.
                    pSubRunsOut[0].backgroundColor = pTL->selectionBackgroundColor;
                    return 1;
                }
                else
                {
                    // The head part of the run is selected, and the tail is deselected.

                    // Head.
                    pSubRunsOut[0].backgroundColor = pTL->selectionBackgroundColor;
                    pSubRunsOut[0].iCharEnd        = iSelectionChar1;
                    pSubRunsOut[0].width           = pSelectionMarker1->relativePosX;
                    pSubRunsOut[0].text            = pTL->text + pSubRunsOut[0].iChar;
                    pSubRunsOut[0].textLength      = pSubRunsOut[0].iCharEnd - pSubRunsOut[0].iChar;
                    
                    // Tail.
                    pSubRunsOut[1].iChar      = iSelectionChar1;
                    pSubRunsOut[1].width      = pRunToSplit->width - pSelectionMarker1->relativePosX;
                    pSubRunsOut[1].posX       = pSubRunsOut[0].posX + pSubRunsOut[0].width;
                    pSubRunsOut[1].text       = pTL->text + pSubRunsOut[1].iChar;
                    pSubRunsOut[1].textLength = pSubRunsOut[1].iCharEnd - pSubRunsOut[1].iChar;

                    return 2;
                }
            }
            else
            {
                // The first part of the run is deselected. There will be at least 2, but possibly 3 sub-runs in this case.
                if (pRunToSplit->iCharEnd <= iSelectionChar1)
                {
                    // The head of the run is deselected and the tail is selected.
                    
                    // Head.
                    pSubRunsOut[0].iCharEnd        = iSelectionChar0;
                    pSubRunsOut[0].width           = pSelectionMarker0->relativePosX;
                    pSubRunsOut[0].text            = pTL->text + pSubRunsOut[0].iChar;
                    pSubRunsOut[0].textLength      = pSubRunsOut[0].iCharEnd - pSubRunsOut[0].iChar;
                    
                    // Tail.
                    pSubRunsOut[1].backgroundColor = pTL->selectionBackgroundColor;
                    pSubRunsOut[1].iChar           = iSelectionChar0;
                    pSubRunsOut[1].width           = pRunToSplit->width - pSubRunsOut[0].width;
                    pSubRunsOut[1].posX            = pSubRunsOut[0].posX + pSubRunsOut[0].width;
                    pSubRunsOut[1].text            = pTL->text + pSubRunsOut[1].iChar;
                    pSubRunsOut[1].textLength      = pSubRunsOut[1].iCharEnd - pSubRunsOut[1].iChar;

                    return 2;
                }
                else
                {
                    // The head and tail are both deselected, and the middle section is selected.

                    // Head.
                    pSubRunsOut[0].iCharEnd   = iSelectionChar0;
                    pSubRunsOut[0].width      = pSelectionMarker0->relativePosX;
                    pSubRunsOut[0].text       = pTL->text + pSubRunsOut[0].iChar;
                    pSubRunsOut[0].textLength = pSubRunsOut[0].iCharEnd - pSubRunsOut[0].iChar;

                    // Mid.
                    pSubRunsOut[1].iChar           = iSelectionChar0;
                    pSubRunsOut[1].iCharEnd        = iSelectionChar1;
                    pSubRunsOut[1].backgroundColor = pTL->selectionBackgroundColor;
                    pSubRunsOut[1].width           = pSelectionMarker1->relativePosX - pSelectionMarker0->relativePosX;
                    pSubRunsOut[1].posX            = pSubRunsOut[0].posX + pSubRunsOut[0].width;
                    pSubRunsOut[1].text            = pTL->text + pSubRunsOut[1].iChar;
                    pSubRunsOut[1].textLength      = pSubRunsOut[1].iCharEnd - pSubRunsOut[1].iChar;

                    // Tail.
                    pSubRunsOut[2].iChar      = iSelectionChar1;
                    pSubRunsOut[2].width      = pRunToSplit->width - pSelectionMarker1->relativePosX;
                    pSubRunsOut[2].posX       = pSubRunsOut[1].posX + pSubRunsOut[1].width;
                    pSubRunsOut[2].text       = pTL->text + pSubRunsOut[2].iChar;
                    pSubRunsOut[2].textLength = pSubRunsOut[2].iCharEnd - pSubRunsOut[2].iChar;

                    return 3;
                }
            }
        }
    }

    // If we get here it means the run is not within the selected region.
    pSubRunsOut[0] = *pRunToSplit;
    return 1;
}

PRIVATE bool easygui_text_layout__is_run_selected(easygui_text_layout* pTL, unsigned int iRun)
{
    if (easygui_text_layout_is_anything_selected(pTL))
    {
        easygui_text_marker* pSelectionMarker0;
        easygui_text_marker* pSelectionMarker1;
        if (!easygui_text_layout__get_selection_markers(pTL, &pSelectionMarker0, &pSelectionMarker1)) {
            return false;
        }

        unsigned int iSelectionChar0 = pTL->pRuns[pSelectionMarker0->iRun].iChar + pSelectionMarker0->iChar;
        unsigned int iSelectionChar1 = pTL->pRuns[pSelectionMarker1->iRun].iChar + pSelectionMarker1->iChar;

        return pTL->pRuns[iRun].iChar < iSelectionChar1 && pTL->pRuns[iRun].iCharEnd > iSelectionChar0;
    }

    return false;
}

PRIVATE bool easygui_text_layout__get_selection_markers(easygui_text_layout* pTL, easygui_text_marker** ppSelectionMarker0Out, easygui_text_marker** ppSelectionMarker1Out)
{
    bool result = false;

    easygui_text_marker* pSelectionMarker0 = NULL;
    easygui_text_marker* pSelectionMarker1 = NULL;
    if (pTL != NULL && easygui_text_layout_is_anything_selected(pTL))
    {
        pSelectionMarker0 = &pTL->selectionAnchor;
        pSelectionMarker1 = &pTL->cursor;
        if (pTL->pRuns[pSelectionMarker0->iRun].iChar + pSelectionMarker0->iChar > pTL->pRuns[pSelectionMarker1->iRun].iChar + pSelectionMarker1->iChar)    
        {
            easygui_text_marker* temp = pSelectionMarker0;
            pSelectionMarker0 = pSelectionMarker1;
            pSelectionMarker1 = temp;
        }

        result = true;
    }

    if (ppSelectionMarker0Out) {
        *ppSelectionMarker0Out = pSelectionMarker0;
    }
    if (ppSelectionMarker1Out) {
        *ppSelectionMarker1Out = pSelectionMarker1;
    }

    return result;
}


PRIVATE bool easygui_text_layout__first_line(easygui_text_layout* pTL, easygui_text_layout_line* pLine)
{
    if (pTL == NULL || pLine == NULL || pTL->runCount == 0) {
        return false;
    }

    pLine->index     = 0;
    pLine->posY      = 0;
    pLine->height    = 0;
    pLine->iFirstRun = 0;
    pLine->iLastRun  = 0;

    // We need to find the last run in the line and the height. The height is determined by the run with the largest height.
    while (pLine->iLastRun < pTL->runCount)
    {
        if (pLine->height < pTL->pRuns[pLine->iLastRun].height) {
            pLine->height = pTL->pRuns[pLine->iLastRun].height;
        }

        pLine->iLastRun += 1;

        if (pTL->pRuns[pLine->iLastRun].iLine != pLine->index) {
            break;
        }
    }

    if (pLine->iLastRun > 0) {
        pLine->iLastRun -= 1;
    }

    return true;
}

PRIVATE bool easygui_text_layout__next_line(easygui_text_layout* pTL, easygui_text_layout_line* pLine)
{
    if (pTL == NULL || pLine == NULL || pTL->runCount == 0) {
        return false;
    }

    // If there's no more runs, there is no next line.
    if (pLine->iLastRun == pTL->runCount - 1) {
        return false;
    }

    pLine->index     += 1;
    pLine->posY      += pLine->height;
    pLine->height    = 0;
    pLine->iFirstRun = pLine->iLastRun + 1;
    pLine->iLastRun  = pLine->iFirstRun;

    while (pLine->iLastRun < pTL->runCount)
    {
        if (pLine->height < pTL->pRuns[pLine->iLastRun].height) {
            pLine->height = pTL->pRuns[pLine->iLastRun].height;
        }

        pLine->iLastRun += 1;

        if (pTL->pRuns[pLine->iLastRun].iLine != pLine->index) {
            break;
        }
    }

    if (pLine->iLastRun > 0) {
        pLine->iLastRun -= 1;
    }

    return true;
}

PRIVATE void easygui_text_layout__trim_undo_stack(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return;
    }

    while (pTL->undoStackCount > pTL->iUndoState)
    {
        unsigned int iLastItem = pTL->undoStackCount - 1;
        
        easygui_text_layout__uninit_undo_state(pTL->pUndoStack + iLastItem);
        pTL->undoStackCount -= 1;
    }
}

PRIVATE bool easygui_text_layout__diff_states(easygui_text_layout_state* pPrevState, easygui_text_layout_state* pCurrentState, easygui_text_layout_undo_state* pUndoStateOut)
{
    if (pPrevState == NULL || pCurrentState == NULL || pUndoStateOut == NULL) {
        return false;
    }

    if (pPrevState->text == NULL || pCurrentState->text == NULL) {
        return false;
    }

    const char* prevText = pPrevState->text;
    const char* currText = pCurrentState->text;
    
    const size_t prevLen = strlen(prevText);
    const size_t currLen = strlen(currText);


    // The first step is to find the position of the first differing character.
    size_t sameChCountStart;
    for (sameChCountStart = 0; sameChCountStart < prevLen && sameChCountStart < currLen; ++sameChCountStart)
    {
        char prevCh = prevText[sameChCountStart];
        char currCh = currText[sameChCountStart];

        if (prevCh != currCh) {
            break;
        }
    }

    // The next step is to find the position of the last differing character.
    size_t sameChCountEnd;
    for (sameChCountEnd = 0; sameChCountEnd < prevLen && sameChCountEnd < currLen; ++sameChCountEnd)
    {
        // Don't move beyond the first differing character.
        if (prevLen - sameChCountEnd <= sameChCountStart ||
            currLen - sameChCountEnd <= sameChCountStart)
        {
            break;
        }

        char prevCh = prevText[prevLen - sameChCountEnd - 1];
        char currCh = currText[currLen - sameChCountEnd - 1];

        if (prevCh != currCh) {
            break;
        }
    }
    
    
    // At this point we know which section of the text is different. We now need to initialize the undo state object.
    pUndoStateOut->diffPos       = sameChCountStart;
    pUndoStateOut->newState      = *pCurrentState;
    pUndoStateOut->newState.text = NULL;
    pUndoStateOut->oldState      = *pPrevState;
    pUndoStateOut->oldState.text = NULL;
    
    size_t oldTextLen = prevLen - sameChCountStart - sameChCountEnd;
    pUndoStateOut->oldText = malloc(oldTextLen + 1);
    strncpy_s(pUndoStateOut->oldText, oldTextLen + 1, prevText + sameChCountStart, oldTextLen);

    size_t newTextLen = currLen - sameChCountStart - sameChCountEnd;
    pUndoStateOut->newText = malloc(newTextLen + 1);
    strncpy_s(pUndoStateOut->newText, newTextLen + 1, currText + sameChCountStart, newTextLen);

    return true;
}

PRIVATE void easygui_text_layout__uninit_undo_state(easygui_text_layout_undo_state* pUndoState)
{
    if (pUndoState == NULL) {
        return;
    }

    free(pUndoState->oldText);
    pUndoState->oldText = NULL;

    free(pUndoState->newText);
    pUndoState->newText = NULL;
}

PRIVATE void easygui_text_layout__push_undo_state(easygui_text_layout* pTL, easygui_text_layout_undo_state* pUndoState)
{
    if (pTL == NULL || pUndoState == NULL) {
        return;
    }

    assert(pTL->iUndoState == pTL->undoStackCount);


    easygui_text_layout_undo_state* pOldStack = pTL->pUndoStack;
    easygui_text_layout_undo_state* pNewStack = malloc(sizeof(*pNewStack) * (pTL->undoStackCount + 1));

    if (pTL->undoStackCount > 0) {
        memcpy(pNewStack, pOldStack, sizeof(*pNewStack) * (pTL->undoStackCount));
    }

    pNewStack[pTL->undoStackCount] = *pUndoState;
    pTL->pUndoStack = pNewStack;
    pTL->undoStackCount += 1;
    pTL->iUndoState += 1;

    free(pOldStack);
}

PRIVATE void easygui_text_layout__apply_undo_state(easygui_text_layout* pTL, easygui_text_layout_undo_state* pUndoState)
{
    if (pTL == NULL || pUndoState == NULL) {
        return;
    }

    // When undoing we want to remove the new text and replace it with the old text.

    // Remove the new text.
    easygui_text_layout_delete_text_range(pTL, pUndoState->diffPos, pUndoState->diffPos + strlen(pUndoState->newText));        // TODO: Replace this with a raw string replace. This will rebuild the text runs which is needlessly inefficient.

    // Insert the old text.
    easygui_text_layout_insert_text(pTL, pUndoState->oldText, pUndoState->diffPos);


    // Markers needs to be updated after refreshing the layout.
    easygui_text_layout__move_marker_to_character(pTL, &pTL->cursor, pUndoState->oldState.cursorPos);
    easygui_text_layout__move_marker_to_character(pTL, &pTL->selectionAnchor, pUndoState->oldState.selectionAnchorPos);

    // The cursor's sticky position needs to be updated whenever the text is edited.
    easygui_text_layout__update_marker_sticky_position(pTL, &pTL->cursor);
    
    // Ensure we mark the text as selected if appropriate.
    pTL->isAnythingSelected = pUndoState->oldState.isAnythingSelected;


    if (pTL->onDirty) {
        pTL->onDirty(pTL, easygui_text_layout__local_rect(pTL));
    }
}

PRIVATE void easygui_text_layout__apply_redo_state(easygui_text_layout* pTL, easygui_text_layout_undo_state* pUndoState)
{
    if (pTL == NULL || pUndoState == NULL) {
        return;
    }

    // An redo is just the opposite of an undo. We want to remove the old text and replace it with the new text.

    // Remove the old text.
    easygui_text_layout_delete_text_range(pTL, pUndoState->diffPos, pUndoState->diffPos + strlen(pUndoState->oldText));        // TODO: Replace this with a raw string replace. This will rebuild the text runs which is needlessly inefficient.

    // Insert the new text.
    easygui_text_layout_insert_text(pTL, pUndoState->newText, pUndoState->diffPos);


    // Markers needs to be updated after refreshing the layout.
    easygui_text_layout__move_marker_to_character(pTL, &pTL->cursor, pUndoState->newState.cursorPos);
    easygui_text_layout__move_marker_to_character(pTL, &pTL->selectionAnchor, pUndoState->newState.selectionAnchorPos);

    // The cursor's sticky position needs to be updated whenever the text is edited.
    easygui_text_layout__update_marker_sticky_position(pTL, &pTL->cursor);

    // Ensure we mark the text as selected if appropriate.
    pTL->isAnythingSelected = pUndoState->newState.isAnythingSelected;


    if (pTL->onDirty) {
        pTL->onDirty(pTL, easygui_text_layout__local_rect(pTL));
    }
}


PRIVATE easygui_rect easygui_text_layout__local_rect(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return easygui_make_rect(0, 0, 0, 0);
    }

    return easygui_make_rect(0, 0, pTL->containerWidth, pTL->containerHeight);
}
