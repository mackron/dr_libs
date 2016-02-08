// Public domain. See "unlicense" statement at the end of this file.

#include "dr_gui_text_layout.h"
#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#ifndef OUT
#define OUT
#endif

#ifndef DO_NOTHING
#define DO_NOTHING
#endif

#ifndef DRGUI_PRIVATE
#define DRGUI_PRIVATE static
#endif

static int drgui__strcpy_s(char* dst, size_t dstSizeInBytes, const char* src)
{
#ifdef _MSC_VER
    return strcpy_s(dst, dstSizeInBytes, src);
#else
    if (dst == 0) {
        return EINVAL;
    }
    if (dstSizeInBytes == 0) {
        return ERANGE;
    }
    if (src == 0) {
        dst[0] = '\0';
        return EINVAL;
    }

    size_t i;
    for (i = 0; i < dstSizeInBytes && src[i] != '\0'; ++i) {
        dst[i] = src[i];
    }

    if (i < dstSizeInBytes) {
        dst[i] = '\0';
        return 0;
    }

    dst[0] = '\0';
    return ERANGE;
#endif
}

static int drgui__strncpy_s(char* dst, size_t dstSizeInBytes, const char* src, size_t count)
{
#ifdef _MSC_VER
    return strncpy_s(dst, dstSizeInBytes, src, count);
#else
    if (dst == 0) {
        return EINVAL;
    }
    if (dstSizeInBytes == 0) {
        return EINVAL;
    }
    if (src == 0) {
        dst[0] = '\0';
        return EINVAL;
    }

    size_t maxcount = count;
    if (count == ((size_t)-1) || count >= dstSizeInBytes) {        // -1 = _TRUNCATE
        maxcount = dstSizeInBytes - 1;
    }

    size_t i;
    for (i = 0; i < maxcount && src[i] != '\0'; ++i) {
        dst[i] = src[i];
    }

    if (src[i] == '\0' || i == count || count == ((size_t)-1)) {
        dst[i] = '\0';
        return 0;
    }

    dst[0] = '\0';
    return ERANGE;
#endif
}


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

} drgui_text_marker;

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

} drgui_text_layout_state;

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
    drgui_text_layout_state oldState;

    /// The state of the text layout at the time the undo point was committed, not including the text. The <text> attribute
    /// of this object is always null.
    drgui_text_layout_state newState;

} drgui_text_layout_undo_state;

struct drgui_text_layout
{
    /// The main text of the layout.
    char* text;

    /// The length of the text.
    size_t textLength;


    /// The function to call when the text layout needs to be redrawn.
    drgui_text_layout_on_dirty_proc onDirty;

    /// The function to call when the content of the text layout changes.
    drgui_text_layout_on_text_changed_proc onTextChanged;

    /// The function to call when the current undo point has changed.
    drgui_text_layout_on_undo_point_changed_proc onUndoPointChanged;


    /// The width of the container.
    float containerWidth;

    /// The height of the container.
    float containerHeight;

    /// The inner offset of the container.
    float innerOffsetX;

    /// The inner offset of the container.
    float innerOffsetY;


    /// The default font.
    drgui_font* pDefaultFont;

    /// The default text color.
    drgui_color defaultTextColor;

    /// The default background color.
    drgui_color defaultBackgroundColor;

    /// The background color to use for selected text.
    drgui_color selectionBackgroundColor;

    /// The background color to use for the line the cursor is currently sitting on.
    drgui_color lineBackgroundColor;

    /// The size of a tab in spaces.
    unsigned int tabSizeInSpaces;

    /// The horizontal alignment.
    drgui_text_layout_alignment horzAlign;

    /// The vertical alignment.
    drgui_text_layout_alignment vertAlign;

    /// The width of the text cursor.
    float cursorWidth;

    /// The color of the text cursor.
    drgui_color cursorColor;

    /// The blink rate in milliseconds of the cursor.
    unsigned int cursorBlinkRate;

    /// The amount of time in milliseconds to toggle the cursor's blink state.
    unsigned int timeToNextCursorBlink;

    /// Whether or not the cursor is showing based on it's blinking state.
    bool isCursorBlinkOn;

    /// Whether or not the cursor is being shown. False by default.
    bool isShowingCursor;


    /// The total width of the text.
    float textBoundsWidth;

    /// The total height of the text.
    float textBoundsHeight;


    /// The cursor.
    drgui_text_marker cursor;

    /// The selection anchor.
    drgui_text_marker selectionAnchor;


    /// The selection mode counter. When this is greater than 0 we are in selection mode, otherwise we are not. This
    /// is incremented by enter_selection_mode() and decremented by leave_selection_mode().
    unsigned int selectionModeCounter;

    /// Whether or not anything is selected.
    bool isAnythingSelected;


    /// The function to call when a text run needs to be painted.
    drgui_text_layout_on_paint_text_proc onPaintText;

    /// The function to call when a rectangle needs to be painted.
    drgui_text_layout_on_paint_rect_proc onPaintRect;

    /// The function to call when the cursor moves.
    drgui_text_layout_on_cursor_move_proc onCursorMove;


    /// The prepared undo/redo state. This will be filled with some state by PrepareUndoRedoPoint() and again with CreateUndoRedoPoint().
    drgui_text_layout_state preparedState;

    /// The undo/redo stack.
    drgui_text_layout_undo_state* pUndoStack;

    /// The number of items in the undo/redo stack.
    unsigned int undoStackCount;

    /// The index of the undo/redo state item we are currently sitting on.
    unsigned int iUndoState;


    /// A pointer to the buffer containing details about every run in the layout.
    drgui_text_run* pRuns;

    /// The number of runs in <pRuns>.
    size_t runCount;

    /// The size of the <pRuns> buffer in drgui_text_run's. This is used to determine whether or not the buffer
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

} drgui_text_layout_line;


/// Performs a complete refresh of the given text layout.
///
/// @remarks
///     This will delete every run and re-create them.
DRGUI_PRIVATE void drgui_text_layout__refresh(drgui_text_layout* pTL);

/// Refreshes the alignment of the given text layout.
DRGUI_PRIVATE void drgui_text_layout__refresh_alignment(drgui_text_layout* pTL);

/// Appends a text run to the list of runs in the given text layout.
DRGUI_PRIVATE void drgui_text_layout__push_text_run(drgui_text_layout* pTL, drgui_text_run* pRun);

/// Clears the internal list of text runs.
DRGUI_PRIVATE void drgui_text_layout__clear_text_runs(drgui_text_layout* pTL);

/// Helper for calculating the offset to apply to each line based on the alignment of the given text layout.
DRGUI_PRIVATE void drgui_text_layout__calculate_line_alignment_offset(drgui_text_layout* pTL, float lineWidth, float* pOffsetXOut, float* pOffsetYOut);

/// Helper for determine whether or not the given text run is whitespace.
DRGUI_PRIVATE bool drgui_text_layout__is_text_run_whitespace(drgui_text_layout* pTL, drgui_text_run* pRun);

/// Helper for calculating the width of a tab.
DRGUI_PRIVATE float drgui_text_layout__get_tab_width(drgui_text_layout* pTL);


/// Finds the line that's closest to the given point relative to the text.
DRGUI_PRIVATE bool drgui_text_layout__find_closest_line_to_point(drgui_text_layout* pTL, float inputPosYRelativeToText, unsigned int* pFirstRunIndexOnLineOut, unsigned int* pLastRunIndexOnLinePlus1Out);

/// Finds the run that's closest to the given point relative to the text.
DRGUI_PRIVATE bool drgui_text_layout__find_closest_run_to_point(drgui_text_layout* pTL, float inputPosXRelativeToText, float inputPosYRelativeToText, unsigned int* pRunIndexOut);

/// Retrieves some basic information about a line, namely the index of the last run on the line, and the line's height.
DRGUI_PRIVATE bool drgui_text_layout__find_line_info(drgui_text_layout* pTL, unsigned int iFirstRunOnLine, unsigned int* pLastRunIndexOnLinePlus1Out, float* pLineHeightOut);

/// Retrieves some basic information about a line by it's index.
DRGUI_PRIVATE bool drgui_text_layout__find_line_info_by_index(drgui_text_layout* pTL, unsigned int iLine, drgui_rect* pRectOut, unsigned int* pFirstRunIndexOut, unsigned int* pLastRunIndexPlus1Out);

/// Finds the last run on the line that the given run is sitting on.
DRGUI_PRIVATE bool drgui_text_layout__find_last_run_on_line_starting_from_run(drgui_text_layout* pTL, unsigned int iRun, unsigned int* pLastRunIndexOnLineOut);

/// Finds the first run on the line that the given run is sitting on.
DRGUI_PRIVATE bool drgui_text_layout__find_first_run_on_line_starting_from_run(drgui_text_layout* pTL, unsigned int iRun, unsigned int* pFirstRunIndexOnLineOut);

/// Finds the run containing the character at the given index.
DRGUI_PRIVATE bool drgui_text_layout__find_run_at_character(drgui_text_layout* pTL, unsigned int iChar, unsigned int* pRunIndexOut);


/// Creates a blank text marker.
DRGUI_PRIVATE drgui_text_marker drgui_text_layout__new_marker();

/// Moves the given text marker to the given point, relative to the container.
DRGUI_PRIVATE bool drgui_text_layout__move_marker_to_point_relative_to_container(drgui_text_layout* pTL, drgui_text_marker* pMarker, float inputPosX, float inputPosY);

/// Retrieves the position of the given text marker relative to the container.
DRGUI_PRIVATE void drgui_text_layout__get_marker_position_relative_to_container(drgui_text_layout* pTL, drgui_text_marker* pMarker, float* pPosXOut, float* pPosYOut);

/// Moves the marker to the given point, relative to the text rectangle.
DRGUI_PRIVATE bool drgui_text_layout__move_marker_to_point(drgui_text_layout* pTL, drgui_text_marker* pMarker, float inputPosXRelativeToText, float inputPosYRelativeToText);

/// Moves the given marker to the left by one character.
DRGUI_PRIVATE bool drgui_text_layout__move_marker_left(drgui_text_layout* pTL, drgui_text_marker* pMarker);

/// Moves the given marker to the right by one character.
DRGUI_PRIVATE bool drgui_text_layout__move_marker_right(drgui_text_layout* pTL, drgui_text_marker* pMarker);

/// Moves the given marker up one line.
DRGUI_PRIVATE bool drgui_text_layout__move_marker_up(drgui_text_layout* pTL, drgui_text_marker* pMarker);

/// Moves the given marker down one line.
DRGUI_PRIVATE bool drgui_text_layout__move_marker_down(drgui_text_layout* pTL, drgui_text_marker* pMarker);

/// Moves the given marker down one line.
DRGUI_PRIVATE bool drgui_text_layout__move_marker_y(drgui_text_layout* pTL, drgui_text_marker* pMarker, int amount);

/// Moves the given marker to the end of the line it's currently sitting on.
DRGUI_PRIVATE bool drgui_text_layout__move_marker_to_end_of_line(drgui_text_layout* pTL, drgui_text_marker* pMarker);

/// Moves the given marker to the start of the line it's currently sitting on.
DRGUI_PRIVATE bool drgui_text_layout__move_marker_to_start_of_line(drgui_text_layout* pTL, drgui_text_marker* pMarker);

/// Moves the given marker to the end of the line at the given index.
DRGUI_PRIVATE bool drgui_text_layout__move_marker_to_end_of_line_by_index(drgui_text_layout* pTL, drgui_text_marker* pMarker, unsigned int iLine);

/// Moves the given marker to the start of the line at the given index.
DRGUI_PRIVATE bool drgui_text_layout__move_marker_to_start_of_line_by_index(drgui_text_layout* pTL, drgui_text_marker* pMarker, unsigned int iLine);

/// Moves the given marker to the end of the text.
DRGUI_PRIVATE bool drgui_text_layout__move_marker_to_end_of_text(drgui_text_layout* pTL, drgui_text_marker* pMarker);

/// Moves the given marker to the start of the text.
DRGUI_PRIVATE bool drgui_text_layout__move_marker_to_start_of_text(drgui_text_layout* pTL, drgui_text_marker* pMarker);

/// Moves the given marker to the last character of the given run.
DRGUI_PRIVATE bool drgui_text_layout__move_marker_to_last_character_of_run(drgui_text_layout* pTL, drgui_text_marker* pMarker, unsigned int iRun);

/// Moves the given marker to the first character of the given run.
DRGUI_PRIVATE bool drgui_text_layout__move_marker_to_first_character_of_run(drgui_text_layout* pTL, drgui_text_marker* pMarker, unsigned int iRun);

/// Moves the given marker to the last character of the previous run.
DRGUI_PRIVATE bool drgui_text_layout__move_marker_to_last_character_of_prev_run(drgui_text_layout* pTL, drgui_text_marker* pMarker);

/// Moves the given marker to the first character of the next run.
DRGUI_PRIVATE bool drgui_text_layout__move_marker_to_first_character_of_next_run(drgui_text_layout* pTL, drgui_text_marker* pMarker);

/// Moves the given marker to the character at the given position.
DRGUI_PRIVATE bool drgui_text_layout__move_marker_to_character(drgui_text_layout* pTL, drgui_text_marker* pMarker, unsigned int iChar);


/// Updates the relative position of the given marker.
///
/// @remarks
///     This assumes the iRun and iChar properties are valid.
DRGUI_PRIVATE bool drgui_text_layout__update_marker_relative_position(drgui_text_layout* pTL, drgui_text_marker* pMarker);

/// Updates the sticky position of the given marker.
DRGUI_PRIVATE void drgui_text_layout__update_marker_sticky_position(drgui_text_layout* pTL, drgui_text_marker* pMarker);


/// Retrieves the index of the character the given marker is located at.
DRGUI_PRIVATE unsigned int drgui_text_layout__get_marker_absolute_char_index(drgui_text_layout* pTL, drgui_text_marker* pMarker);


/// Helper function for determining whether or not there is any spacing between the selection markers.
DRGUI_PRIVATE bool drgui_text_layout__has_spacing_between_selection_markers(drgui_text_layout* pTL);

/// Splits the given run into sub-runs based on the current selection rectangle. Returns the sub-run count.
DRGUI_PRIVATE unsigned int drgui_text_layout__split_text_run_by_selection(drgui_text_layout* pTL, drgui_text_run* pRunToSplit, drgui_text_run pSubRunsOut[3]);

/// Determines whether or not the run at the given index is selected.
//DRGUI_PRIVATE bool drgui_text_layout__is_run_selected(drgui_text_layout* pTL, unsigned int iRun);

/// Retrieves pointers to the selection markers in the correct order.
DRGUI_PRIVATE bool drgui_text_layout__get_selection_markers(drgui_text_layout* pTL, drgui_text_marker** ppSelectionMarker0Out, drgui_text_marker** ppSelectionMarker1Out);


/// Retrieves an iterator to the first line in the text layout.
DRGUI_PRIVATE bool drgui_text_layout__first_line(drgui_text_layout* pTL, drgui_text_layout_line* pLine);

/// Retrieves an iterator to the next line in the text layout.
DRGUI_PRIVATE bool drgui_text_layout__next_line(drgui_text_layout* pTL, drgui_text_layout_line* pLine);


/// Removes the undo/redo state stack items after the current undo/redo point.
DRGUI_PRIVATE void drgui_text_layout__trim_undo_stack(drgui_text_layout* pTL);

/// Initializes the given undo state object by diff-ing the given layout states.
DRGUI_PRIVATE bool drgui_text_layout__diff_states(drgui_text_layout_state* pPrevState, drgui_text_layout_state* pCurrentState, drgui_text_layout_undo_state* pUndoStateOut);

/// Uninitializes the given undo state object. This basically just free's the internal string.
DRGUI_PRIVATE void drgui_text_layout__uninit_undo_state(drgui_text_layout_undo_state* pUndoState);

/// Pushes an undo state onto the undo stack.
DRGUI_PRIVATE void drgui_text_layout__push_undo_state(drgui_text_layout* pTL, drgui_text_layout_undo_state* pUndoState);

/// Applies the given undo state.
DRGUI_PRIVATE void drgui_text_layout__apply_undo_state(drgui_text_layout* pTL, drgui_text_layout_undo_state* pUndoState);

/// Applies the given undo state as a redo operation.
DRGUI_PRIVATE void drgui_text_layout__apply_redo_state(drgui_text_layout* pTL, drgui_text_layout_undo_state* pUndoState);


/// Retrieves a rectangle relative to the given text layout that's equal to the size of the container.
DRGUI_PRIVATE drgui_rect drgui_text_layout__local_rect(drgui_text_layout* pTL);


/// Called when the cursor moves.
DRGUI_PRIVATE void drgui_text_layout__on_cursor_move(drgui_text_layout* pTL);



drgui_text_layout* drgui_create_text_layout(drgui_context* pContext, size_t extraDataSize, void* pExtraData)
{
    if (pContext == NULL) {
        return NULL;
    }

    drgui_text_layout* pTL = malloc(sizeof(drgui_text_layout) - sizeof(pTL->pExtraData) + extraDataSize);
    if (pTL == NULL) {
        return NULL;
    }

    pTL->text                       = NULL;
    pTL->textLength                 = 0;
    pTL->onDirty                    = NULL;
    pTL->onTextChanged              = NULL;
    pTL->onUndoPointChanged         = NULL;
    pTL->containerWidth             = 0;
    pTL->containerHeight            = 0;
    pTL->innerOffsetX               = 0;
    pTL->innerOffsetY               = 0;
    pTL->pDefaultFont               = NULL;
    pTL->defaultTextColor           = drgui_rgb(224, 224, 224);
    pTL->defaultBackgroundColor     = drgui_rgb(48, 48, 48);
    pTL->selectionBackgroundColor   = drgui_rgb(64, 128, 192);
    pTL->lineBackgroundColor        = drgui_rgb(40, 40, 40);
    pTL->tabSizeInSpaces            = 4;
    pTL->horzAlign                  = drgui_text_layout_alignment_left;
    pTL->vertAlign                  = drgui_text_layout_alignment_top;
    pTL->cursorWidth                = 1;
    pTL->cursorColor                = drgui_rgb(224, 224, 224);
    pTL->cursorBlinkRate            = 500;
    pTL->timeToNextCursorBlink      = pTL->cursorBlinkRate;
    pTL->isCursorBlinkOn            = true;
    pTL->isShowingCursor            = false;
    pTL->textBoundsWidth            = 0;
    pTL->textBoundsHeight           = 0;
    pTL->cursor                     = drgui_text_layout__new_marker();
    pTL->selectionAnchor            = drgui_text_layout__new_marker();
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

void drgui_delete_text_layout(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return;
    }

    drgui_text_layout_clear_undo_stack(pTL);

    free(pTL->pRuns);
    free(pTL->preparedState.text);
    free(pTL->text);
    free(pTL);
}


size_t drgui_text_layout_get_extra_data_size(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return 0;
    }

    return pTL->extraDataSize;
}

void* drgui_text_layout_get_extra_data(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return NULL;
    }

    return pTL->pExtraData;
}


void drgui_text_layout_set_text(drgui_text_layout* pTL, const char* text)
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
    drgui_text_layout__refresh(pTL);

    // If the position of the cursor is past the last character we'll need to move it.
    if (drgui_text_layout__get_marker_absolute_char_index(pTL, &pTL->cursor) >= pTL->textLength) {
        drgui_text_layout_move_cursor_to_end_of_text(pTL);
    }

    if (pTL->onTextChanged) {
        pTL->onTextChanged(pTL);
    }

    if (pTL->onDirty) {
        pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
    }
}

size_t drgui_text_layout_get_text(drgui_text_layout* pTL, char* textOut, size_t textOutSize)
{
    if (pTL == NULL) {
        return 0;
    }

    if (textOut == NULL) {
        return pTL->textLength;
    }


    if (drgui__strcpy_s(textOut, textOutSize, (pTL->text != NULL) ? pTL->text : "") == 0) {
        return pTL->textLength;
    }

    return 0;   // Error with strcpy_s().
}


void drgui_text_layout_set_on_dirty(drgui_text_layout* pTL, drgui_text_layout_on_dirty_proc proc)
{
    if (pTL == NULL) {
        return;
    }

    pTL->onDirty = proc;
}

void drgui_text_layout_set_on_text_changed(drgui_text_layout* pTL, drgui_text_layout_on_text_changed_proc proc)
{
    if (pTL == NULL) {
        return;
    }

    pTL->onTextChanged = proc;
}

void drgui_text_layout_set_on_undo_point_changed(drgui_text_layout* pTL, drgui_text_layout_on_undo_point_changed_proc proc)
{
    if (pTL == NULL) {
        return;
    }

    pTL->onUndoPointChanged = proc;
}


void drgui_text_layout_set_container_size(drgui_text_layout* pTL, float containerWidth, float containerHeight)
{
    if (pTL == NULL) {
        return;
    }

    pTL->containerWidth  = containerWidth;
    pTL->containerHeight = containerHeight;

    if (pTL->onDirty) {
        pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
    }
}

void drgui_text_layout_get_container_size(drgui_text_layout* pTL, float* pContainerWidthOut, float* pContainerHeightOut)
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

float drgui_text_layout_get_container_width(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return 0;
    }

    return pTL->containerWidth;
}

float drgui_text_layout_get_container_height(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return 0;
    }

    return pTL->containerHeight;
}


void drgui_text_layout_set_inner_offset(drgui_text_layout* pTL, float innerOffsetX, float innerOffsetY)
{
    if (pTL == NULL) {
        return;
    }

    pTL->innerOffsetX = innerOffsetX;
    pTL->innerOffsetY = innerOffsetY;

    if (pTL->onDirty) {
        pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
    }
}

void drgui_text_layout_set_inner_offset_x(drgui_text_layout* pTL, float innerOffsetX)
{
    if (pTL == NULL) {
        return;
    }

    pTL->innerOffsetX = innerOffsetX;

    if (pTL->onDirty) {
        pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
    }
}

void drgui_text_layout_set_inner_offset_y(drgui_text_layout* pTL, float innerOffsetY)
{
    if (pTL == NULL) {
        return;
    }

    pTL->innerOffsetY = innerOffsetY;

    if (pTL->onDirty) {
        pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
    }
}

void drgui_text_layout_get_inner_offset(drgui_text_layout* pTL, float* pInnerOffsetX, float* pInnerOffsetY)
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

float drgui_text_layout_get_inner_offset_x(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return 0;
    }

    return pTL->innerOffsetX;
}

float drgui_text_layout_get_inner_offset_y(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return 0;
    }

    return pTL->innerOffsetY;
}


void drgui_text_layout_set_default_font(drgui_text_layout* pTL, drgui_font* pFont)
{
    if (pTL == NULL) {
        return;
    }

    pTL->pDefaultFont = pFont;

    // A change in font requires a layout refresh.
    drgui_text_layout__refresh(pTL);

    if (pTL->onDirty) {
        pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
    }
}

drgui_font* drgui_text_layout_get_default_font(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return NULL;
    }

    return pTL->pDefaultFont;
}

void drgui_text_layout_set_default_text_color(drgui_text_layout* pTL, drgui_color color)
{
    if (pTL == NULL) {
        return;
    }

    pTL->defaultTextColor = color;

    if (pTL->onDirty) {
        pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
    }
}

drgui_color drgui_text_layout_get_default_text_color(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return drgui_rgb(0, 0, 0);
    }

    return pTL->defaultTextColor;
}

void drgui_text_layout_set_default_bg_color(drgui_text_layout* pTL, drgui_color color)
{
    if (pTL == NULL) {
        return;
    }

    pTL->defaultBackgroundColor = color;

    if (pTL->onDirty) {
        pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
    }
}

drgui_color drgui_text_layout_get_default_bg_color(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return drgui_rgb(0, 0, 0);
    }

    return pTL->defaultBackgroundColor;
}

void drgui_text_layout_set_active_line_bg_color(drgui_text_layout* pTL, drgui_color color)
{
    if (pTL == NULL) {
        return;
    }

    pTL->lineBackgroundColor = color;

    if (pTL->onDirty) {
        pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
    }
}

drgui_color drgui_text_layout_get_active_line_bg_color(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return drgui_rgb(0, 0, 0);
    }

    return pTL->lineBackgroundColor;
}


void drgui_text_layout_set_tab_size(drgui_text_layout* pTL, unsigned int sizeInSpaces)
{
    if (pTL == NULL) {
        return;
    }

    if (pTL->tabSizeInSpaces != sizeInSpaces)
    {
        pTL->tabSizeInSpaces = sizeInSpaces;
        drgui_text_layout__refresh(pTL);

        if (pTL->onDirty) {
            pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
        }
    }
}

unsigned int drgui_text_layout_get_tab_size(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return 0;
    }

    return pTL->tabSizeInSpaces;
}


void drgui_text_layout_set_horizontal_align(drgui_text_layout* pTL, drgui_text_layout_alignment alignment)
{
    if (pTL == NULL) {
        return;
    }

    if (pTL->horzAlign != alignment)
    {
        pTL->horzAlign = alignment;
        drgui_text_layout__refresh_alignment(pTL);

        if (pTL->onDirty) {
            pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
        }
    }
}

drgui_text_layout_alignment drgui_text_layout_get_horizontal_align(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return drgui_text_layout_alignment_left;
    }

    return pTL->horzAlign;
}

void drgui_text_layout_set_vertical_align(drgui_text_layout* pTL, drgui_text_layout_alignment alignment)
{
    if (pTL == NULL) {
        return;
    }

    if (pTL->vertAlign != alignment)
    {
        pTL->vertAlign = alignment;
        drgui_text_layout__refresh_alignment(pTL);

        if (pTL->onDirty) {
            pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
        }
    }
}

drgui_text_layout_alignment drgui_text_layout_get_vertical_align(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return drgui_text_layout_alignment_top;
    }

    return pTL->vertAlign;
}


drgui_rect drgui_text_layout_get_text_rect_relative_to_bounds(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return drgui_make_rect(0, 0, 0, 0);
    }

    drgui_rect rect;
    rect.left = 0;
    rect.top  = 0;


    switch (pTL->horzAlign)
    {
    case drgui_text_layout_alignment_right:
        {
            rect.left = pTL->containerWidth - pTL->textBoundsWidth;
            break;
        }

    case drgui_text_layout_alignment_center:
        {
            rect.left = (pTL->containerWidth - pTL->textBoundsWidth) / 2;
            break;
        }

    case drgui_text_layout_alignment_left:
    case drgui_text_layout_alignment_top:     // Invalid for horizontal align.
    case drgui_text_layout_alignment_bottom:  // Invalid for horizontal align.
    default:
        {
            break;
        }
    }


    switch (pTL->vertAlign)
    {
    case drgui_text_layout_alignment_bottom:
        {
            rect.top = pTL->containerHeight - pTL->textBoundsHeight;
            break;
        }

    case drgui_text_layout_alignment_center:
        {
            rect.top = (pTL->containerHeight - pTL->textBoundsHeight) / 2;
            break;
        }

    case drgui_text_layout_alignment_top:
    case drgui_text_layout_alignment_left:    // Invalid for vertical align.
    case drgui_text_layout_alignment_right:   // Invalid for vertical align.
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


void drgui_text_layout_set_cursor_width(drgui_text_layout* pTL, float cursorWidth)
{
    if (pTL == NULL) {
        return;
    }

    drgui_rect oldCursorRect = drgui_text_layout_get_cursor_rect(pTL);
    pTL->cursorWidth = cursorWidth;

    if (pTL->onDirty) {
        pTL->onDirty(pTL, drgui_rect_union(oldCursorRect, drgui_text_layout_get_cursor_rect(pTL)));
    }
}

float drgui_text_layout_get_cursor_width(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return 0;
    }

    return pTL->cursorWidth;
}

void drgui_text_layout_set_cursor_color(drgui_text_layout* pTL, drgui_color cursorColor)
{
    if (pTL == NULL) {
        return;
    }

    pTL->cursorColor = cursorColor;

    if (pTL->onDirty) {
        pTL->onDirty(pTL, drgui_text_layout_get_cursor_rect(pTL));
    }
}

drgui_color drgui_text_layout_get_cursor_color(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return drgui_rgb(0, 0, 0);
    }

    return pTL->cursorColor;
}

void drgui_text_layout_set_cursor_blink_rate(drgui_text_layout* pTL, unsigned int blinkRateInMilliseconds)
{
    if (pTL == NULL) {
        return;
    }

    pTL->cursorBlinkRate = blinkRateInMilliseconds;
}

unsigned int drgui_text_layout_get_cursor_blink_rate(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return 0;
    }

    return pTL->cursorBlinkRate;
}

void drgui_text_layout_show_cursor(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return;
    }

    if (!pTL->isShowingCursor)
    {
        pTL->isShowingCursor = true;

        pTL->timeToNextCursorBlink = pTL->cursorBlinkRate;
        pTL->isCursorBlinkOn = true;

        if (pTL->onDirty) {
            pTL->onDirty(pTL, drgui_text_layout_get_cursor_rect(pTL));
        }
    }
}

void drgui_text_layout_hide_cursor(drgui_text_layout* pTL)
{
    if (pTL->isShowingCursor)
    {
        pTL->isShowingCursor = false;

        if (pTL->onDirty) {
            pTL->onDirty(pTL, drgui_text_layout_get_cursor_rect(pTL));
        }
    }
}

bool drgui_text_layout_is_showing_cursor(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return false;
    }

    return pTL->isShowingCursor;
}

void drgui_text_layout_move_cursor_to_point(drgui_text_layout* pTL, float posX, float posY)
{
    if (pTL == NULL) {
        return;
    }

    unsigned int iRunOld  = pTL->cursor.iRun;
    unsigned int iCharOld = pTL->cursor.iChar;
    drgui_text_layout__move_marker_to_point_relative_to_container(pTL, &pTL->cursor, posX, posY);

    if (iRunOld != pTL->cursor.iRun || iCharOld != pTL->cursor.iChar)
    {
        drgui_text_layout__on_cursor_move(pTL);

        if (pTL->onDirty) {
            pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
        }
    }


    if (drgui_text_layout_is_in_selection_mode(pTL)) {
        pTL->isAnythingSelected = drgui_text_layout__has_spacing_between_selection_markers(pTL);
    }
}

void drgui_text_layout_get_cursor_position(drgui_text_layout* pTL, float* pPosXOut, float* pPosYOut)
{
    if (pTL == NULL) {
        return;
    }

    drgui_text_layout__get_marker_position_relative_to_container(pTL, &pTL->cursor, pPosXOut, pPosYOut);
}

drgui_rect drgui_text_layout_get_cursor_rect(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return drgui_make_rect(0, 0, 0, 0);
    }

    drgui_rect lineRect = drgui_make_rect(0, 0, 0, 0);

    if (pTL->runCount > 0)
    {
        drgui_text_layout__find_line_info_by_index(pTL, pTL->pRuns[pTL->cursor.iRun].iLine, &lineRect, NULL, NULL);
    }
    else if (pTL->pDefaultFont != NULL)
    {
        const float scaleX = 1;
        const float scaleY = 1;

        drgui_font_metrics defaultFontMetrics;
        drgui_get_font_metrics(pTL->pDefaultFont, scaleX, scaleY, &defaultFontMetrics);

        lineRect.bottom = (float)defaultFontMetrics.lineHeight;
    }



    float cursorPosX;
    float cursorPosY;
    drgui_text_layout_get_cursor_position(pTL, &cursorPosX, &cursorPosY);

    return drgui_make_rect(cursorPosX, cursorPosY, cursorPosX + pTL->cursorWidth, cursorPosY + (lineRect.bottom - lineRect.top));
}

unsigned int drgui_text_layout_get_cursor_line(drgui_text_layout* pTL)
{
    if (pTL == NULL || pTL->runCount == 0) {
        return 0;
    }

    return pTL->pRuns[pTL->cursor.iRun].iLine;
}

unsigned int drgui_text_layout_get_cursor_column(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return 0;
    }

    float scaleX = 1;
    float scaleY = 1;

    float posX;
    float posY;
    drgui_text_layout_get_cursor_position(pTL, &posX, &posY);

    drgui_font_metrics fontMetrics;
    drgui_get_font_metrics(pTL->pDefaultFont, scaleX, scaleY, &fontMetrics);

    return (unsigned int)((int)posX / fontMetrics.spaceWidth);
}

unsigned int drgui_text_layout_get_cursor_character(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return 0;
    }

    return drgui_text_layout__get_marker_absolute_char_index(pTL, &pTL->cursor);
}

bool drgui_text_layout_move_cursor_left(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return false;
    }

    unsigned int iRunOld  = pTL->cursor.iRun;
    unsigned int iCharOld = pTL->cursor.iChar;
    if (drgui_text_layout__move_marker_left(pTL, &pTL->cursor)) {
        if (drgui_text_layout_is_in_selection_mode(pTL)) {
            pTL->isAnythingSelected = drgui_text_layout__has_spacing_between_selection_markers(pTL);
        }

        if (iRunOld != pTL->cursor.iRun || iCharOld != pTL->cursor.iChar) {
            drgui_text_layout__on_cursor_move(pTL);

            if (pTL->onDirty) {
                pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
            }
        }

        return true;
    }

    return false;
}

bool drgui_text_layout_move_cursor_right(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return false;
    }

    unsigned int iRunOld  = pTL->cursor.iRun;
    unsigned int iCharOld = pTL->cursor.iChar;
    if (drgui_text_layout__move_marker_right(pTL, &pTL->cursor)) {
        if (drgui_text_layout_is_in_selection_mode(pTL)) {
            pTL->isAnythingSelected = drgui_text_layout__has_spacing_between_selection_markers(pTL);
        }

        if (iRunOld != pTL->cursor.iRun || iCharOld != pTL->cursor.iChar) {
            drgui_text_layout__on_cursor_move(pTL);

            if (pTL->onDirty) {
                pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
            }
        }

        return true;
    }

    return false;
}

bool drgui_text_layout_move_cursor_up(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return false;
    }

    unsigned int iRunOld  = pTL->cursor.iRun;
    unsigned int iCharOld = pTL->cursor.iChar;
    if (drgui_text_layout__move_marker_up(pTL, &pTL->cursor)) {
        if (drgui_text_layout_is_in_selection_mode(pTL)) {
            pTL->isAnythingSelected = drgui_text_layout__has_spacing_between_selection_markers(pTL);
        }

        if (iRunOld != pTL->cursor.iRun || iCharOld != pTL->cursor.iChar) {
            drgui_text_layout__on_cursor_move(pTL);

            if (pTL->onDirty) {
                pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
            }
        }

        return true;
    }

    return false;
}

bool drgui_text_layout_move_cursor_down(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return false;
    }

    unsigned int iRunOld  = pTL->cursor.iRun;
    unsigned int iCharOld = pTL->cursor.iChar;
    if (drgui_text_layout__move_marker_down(pTL, &pTL->cursor)) {
        if (drgui_text_layout_is_in_selection_mode(pTL)) {
            pTL->isAnythingSelected = drgui_text_layout__has_spacing_between_selection_markers(pTL);
        }

        if (iRunOld != pTL->cursor.iRun || iCharOld != pTL->cursor.iChar) {
            drgui_text_layout__on_cursor_move(pTL);

            if (pTL->onDirty) {
                pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
            }
        }

        return true;
    }

    return false;
}

bool drgui_text_layout_move_cursor_y(drgui_text_layout* pTL, int amount)
{
    if (pTL == NULL) {
        return false;
    }

    unsigned int iRunOld  = pTL->cursor.iRun;
    unsigned int iCharOld = pTL->cursor.iChar;
    if (drgui_text_layout__move_marker_y(pTL, &pTL->cursor, amount)) {
        if (drgui_text_layout_is_in_selection_mode(pTL)) {
            pTL->isAnythingSelected = drgui_text_layout__has_spacing_between_selection_markers(pTL);
        }

        if (iRunOld != pTL->cursor.iRun || iCharOld != pTL->cursor.iChar) {
            drgui_text_layout__on_cursor_move(pTL);

            if (pTL->onDirty) {
                pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
            }
        }

        return true;
    }

    return false;
}

bool drgui_text_layout_move_cursor_to_end_of_line(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return false;
    }

    unsigned int iRunOld  = pTL->cursor.iRun;
    unsigned int iCharOld = pTL->cursor.iChar;
    if (drgui_text_layout__move_marker_to_end_of_line(pTL, &pTL->cursor)) {
        if (drgui_text_layout_is_in_selection_mode(pTL)) {
            pTL->isAnythingSelected = drgui_text_layout__has_spacing_between_selection_markers(pTL);
        }

        if (iRunOld != pTL->cursor.iRun || iCharOld != pTL->cursor.iChar) {
            drgui_text_layout__on_cursor_move(pTL);

            if (pTL->onDirty) {
                pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
            }
        }

        return true;
    }

    return false;
}

bool drgui_text_layout_move_cursor_to_start_of_line(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return false;
    }

    unsigned int iRunOld  = pTL->cursor.iRun;
    unsigned int iCharOld = pTL->cursor.iChar;
    if (drgui_text_layout__move_marker_to_start_of_line(pTL, &pTL->cursor)) {
        if (drgui_text_layout_is_in_selection_mode(pTL)) {
            pTL->isAnythingSelected = drgui_text_layout__has_spacing_between_selection_markers(pTL);
        }

        if (iRunOld != pTL->cursor.iRun || iCharOld != pTL->cursor.iChar) {
            drgui_text_layout__on_cursor_move(pTL);

            if (pTL->onDirty) {
                pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
            }
        }

        return true;
    }

    return false;
}

bool drgui_text_layout_move_cursor_to_end_of_line_by_index(drgui_text_layout* pTL, unsigned int iLine)
{
    if (pTL == NULL) {
        return false;
    }

    unsigned int iRunOld  = pTL->cursor.iRun;
    unsigned int iCharOld = pTL->cursor.iChar;
    if (drgui_text_layout__move_marker_to_end_of_line_by_index(pTL, &pTL->cursor, iLine)) {
        if (drgui_text_layout_is_in_selection_mode(pTL)) {
            pTL->isAnythingSelected = drgui_text_layout__has_spacing_between_selection_markers(pTL);
        }

        if (iRunOld != pTL->cursor.iRun || iCharOld != pTL->cursor.iChar) {
            drgui_text_layout__on_cursor_move(pTL);

            if (pTL->onDirty) {
                pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
            }
        }

        return true;
    }

    return false;
}

bool drgui_text_layout_move_cursor_to_start_of_line_by_index(drgui_text_layout* pTL, unsigned int iLine)
{
    if (pTL == NULL) {
        return false;
    }

    unsigned int iRunOld  = pTL->cursor.iRun;
    unsigned int iCharOld = pTL->cursor.iChar;
    if (drgui_text_layout__move_marker_to_start_of_line_by_index(pTL, &pTL->cursor, iLine)) {
        if (drgui_text_layout_is_in_selection_mode(pTL)) {
            pTL->isAnythingSelected = drgui_text_layout__has_spacing_between_selection_markers(pTL);
        }

        if (iRunOld != pTL->cursor.iRun || iCharOld != pTL->cursor.iChar) {
            drgui_text_layout__on_cursor_move(pTL);

            if (pTL->onDirty) {
                pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
            }
        }

        return true;
    }

    return false;
}

bool drgui_text_layout_move_cursor_to_end_of_text(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return false;
    }

    unsigned int iRunOld  = pTL->cursor.iRun;
    unsigned int iCharOld = pTL->cursor.iChar;
    if (drgui_text_layout__move_marker_to_end_of_text(pTL, &pTL->cursor)) {
        if (drgui_text_layout_is_in_selection_mode(pTL)) {
            pTL->isAnythingSelected = drgui_text_layout__has_spacing_between_selection_markers(pTL);
        }

        if (iRunOld != pTL->cursor.iRun || iCharOld != pTL->cursor.iChar) {
            drgui_text_layout__on_cursor_move(pTL);

            if (pTL->onDirty) {
                pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
            }
        }

        return true;
    }

    return false;
}

bool drgui_text_layout_move_cursor_to_start_of_text(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return false;
    }

    unsigned int iRunOld  = pTL->cursor.iRun;
    unsigned int iCharOld = pTL->cursor.iChar;
    if (drgui_text_layout__move_marker_to_start_of_text(pTL, &pTL->cursor)) {
        if (drgui_text_layout_is_in_selection_mode(pTL)) {
            pTL->isAnythingSelected = drgui_text_layout__has_spacing_between_selection_markers(pTL);
        }

        if (iRunOld != pTL->cursor.iRun || iCharOld != pTL->cursor.iChar) {
            drgui_text_layout__on_cursor_move(pTL);

            if (pTL->onDirty) {
                pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
            }
        }

        return true;
    }

    return false;
}

void drgui_text_layout_move_cursor_to_start_of_selection(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return;
    }

    drgui_text_marker* pSelectionMarker0;
    drgui_text_marker* pSelectionMarker1;
    if (drgui_text_layout__get_selection_markers(pTL, &pSelectionMarker0, &pSelectionMarker1))
    {
        pTL->cursor = *pSelectionMarker0;
        pTL->isAnythingSelected = drgui_text_layout__has_spacing_between_selection_markers(pTL);

        if (pTL->onDirty) {
            pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
        }
    }
}

void drgui_text_layout_move_cursor_to_end_of_selection(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return;
    }

    drgui_text_marker* pSelectionMarker0;
    drgui_text_marker* pSelectionMarker1;
    if (drgui_text_layout__get_selection_markers(pTL, &pSelectionMarker0, &pSelectionMarker1))
    {
        pTL->cursor = *pSelectionMarker1;
        pTL->isAnythingSelected = drgui_text_layout__has_spacing_between_selection_markers(pTL);

        if (pTL->onDirty) {
            pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
        }
    }
}

void drgui_text_layout_move_cursor_to_character(drgui_text_layout* pTL, unsigned int characterIndex)
{
    if (pTL == NULL) {
        return;
    }

    unsigned int iRunOld  = pTL->cursor.iRun;
    unsigned int iCharOld = pTL->cursor.iChar;
    if (drgui_text_layout__move_marker_to_character(pTL, &pTL->cursor, characterIndex)) {
        if (drgui_text_layout_is_in_selection_mode(pTL)) {
            pTL->isAnythingSelected = drgui_text_layout__has_spacing_between_selection_markers(pTL);
        }

        if (iRunOld != pTL->cursor.iRun || iCharOld != pTL->cursor.iChar) {
            drgui_text_layout__on_cursor_move(pTL);

            if (pTL->onDirty) {
                pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
            }
        }
    }
}

bool drgui_text_layout_is_cursor_at_start_of_selection(drgui_text_layout* pTL)
{
    drgui_text_marker* pSelectionMarker0;
    drgui_text_marker* pSelectionMarker1;
    if (drgui_text_layout__get_selection_markers(pTL, &pSelectionMarker0, &pSelectionMarker1)) {
        return &pTL->cursor == pSelectionMarker0;
    }

    return false;
}

bool drgui_text_layout_is_cursor_at_end_of_selection(drgui_text_layout* pTL)
{
    drgui_text_marker* pSelectionMarker0;
    drgui_text_marker* pSelectionMarker1;
    if (drgui_text_layout__get_selection_markers(pTL, &pSelectionMarker0, &pSelectionMarker1)) {
        return &pTL->cursor == pSelectionMarker1;
    }

    return false;
}

void drgui_text_layout_swap_selection_markers(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return;
    }

    drgui_text_marker* pSelectionMarker0;
    drgui_text_marker* pSelectionMarker1;
    if (drgui_text_layout__get_selection_markers(pTL, &pSelectionMarker0, &pSelectionMarker1))
    {
        unsigned int iRunOld  = pTL->cursor.iRun;
        unsigned int iCharOld = pTL->cursor.iChar;

        drgui_text_marker temp = *pSelectionMarker0;
        *pSelectionMarker0 = *pSelectionMarker1;
        *pSelectionMarker1 = temp;

        if (iRunOld != pTL->cursor.iRun || iCharOld != pTL->cursor.iChar) {
            drgui_text_layout__on_cursor_move(pTL);

            if (pTL->onDirty) {
                pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
            }
        }
    }
}

void drgui_text_layout_set_on_cursor_move(drgui_text_layout* pTL, drgui_text_layout_on_cursor_move_proc proc)
{
    if (pTL == NULL) {
        return;
    }

    pTL->onCursorMove = proc;
}


bool drgui_text_layout_insert_character(drgui_text_layout* pTL, unsigned int character, unsigned int insertIndex)
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
    drgui_text_layout__refresh(pTL);

    if (pTL->onTextChanged) {
        pTL->onTextChanged(pTL);
    }

    if (pTL->onDirty) {
        pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
    }

    return true;
}

bool drgui_text_layout_insert_text(drgui_text_layout* pTL, const char* text, unsigned int insertIndex)
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
    drgui_text_layout__refresh(pTL);

    if (pTL->onTextChanged) {
        pTL->onTextChanged(pTL);
    }

    if (pTL->onDirty) {
        pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
    }

    return true;
}

bool drgui_text_layout_delete_text_range(drgui_text_layout* pTL, unsigned int iFirstCh, unsigned int iLastChPlus1)
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
        drgui_text_layout__refresh(pTL);

        if (pTL->onTextChanged) {
            pTL->onTextChanged(pTL);
        }

        if (pTL->onDirty) {
            pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
        }

        return true;
    }

    return false;
}

bool drgui_text_layout_insert_character_at_cursor(drgui_text_layout* pTL, unsigned int character)
{
    if (pTL == NULL) {
        return false;
    }

    unsigned int iAbsoluteMarkerChar = 0;

    drgui_text_run* pRun = pTL->pRuns + pTL->cursor.iRun;
    if (pTL->runCount > 0 && pRun != NULL) {
        iAbsoluteMarkerChar = pRun->iChar + pTL->cursor.iChar;
    }


    drgui_text_layout_insert_character(pTL, character, iAbsoluteMarkerChar);


    // The marker needs to be updated based on the new layout and it's new position, which is one character ahead.
    drgui_text_layout__move_marker_to_character(pTL, &pTL->cursor, iAbsoluteMarkerChar + 1);

    // The cursor's sticky position needs to be updated whenever the text is edited.
    drgui_text_layout__update_marker_sticky_position(pTL, &pTL->cursor);


    drgui_text_layout__on_cursor_move(pTL);

    return true;
}

bool drgui_text_layout_insert_text_at_cursor(drgui_text_layout* pTL, const char* text)
{
    if (pTL == NULL || text == NULL) {
        return false;
    }

    unsigned int cursorPos = drgui_text_layout__get_marker_absolute_char_index(pTL, &pTL->cursor);
    drgui_text_layout_insert_text(pTL, text, cursorPos);


    // The marker needs to be updated based on the new layout and it's new position, which is one character ahead.
    drgui_text_layout__move_marker_to_character(pTL, &pTL->cursor, cursorPos + strlen(text));

    // The cursor's sticky position needs to be updated whenever the text is edited.
    drgui_text_layout__update_marker_sticky_position(pTL, &pTL->cursor);


    drgui_text_layout__on_cursor_move(pTL);

    return true;
}

bool drgui_text_layout_delete_character_to_left_of_cursor(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return false;;
    }

    // We just move the cursor to the left, and then delete the character to the right.
    if (drgui_text_layout_move_cursor_left(pTL)) {
        drgui_text_layout_delete_character_to_right_of_cursor(pTL);
        return true;
    }

    return false;
}

bool drgui_text_layout_delete_character_to_right_of_cursor(drgui_text_layout* pTL)
{
    if (pTL == NULL || pTL->runCount == 0) {
        return false;
    }

    drgui_text_run* pRun = pTL->pRuns + pTL->cursor.iRun;
    unsigned int iAbsoluteMarkerChar = pRun->iChar + pTL->cursor.iChar;

    if (iAbsoluteMarkerChar < pTL->textLength)
    {
        // TODO: Add proper support for UTF-8.
        memmove(pTL->text + iAbsoluteMarkerChar, pTL->text + iAbsoluteMarkerChar + 1, pTL->textLength - iAbsoluteMarkerChar);
        pTL->textLength -= 1;
        pTL->text[pTL->textLength] = '\0';



        // The layout will have changed.
        drgui_text_layout__refresh(pTL);

        if (pTL->onTextChanged) {
            pTL->onTextChanged(pTL);
        }

        if (pTL->onDirty) {
            pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
        }

        return true;
    }

    return false;
}

bool drgui_text_layout_delete_selected_text(drgui_text_layout* pTL)
{
    // Don't do anything if nothing is selected.
    if (!drgui_text_layout_is_anything_selected(pTL)) {
        return false;
    }

    drgui_text_marker* pSelectionMarker0 = &pTL->selectionAnchor;
    drgui_text_marker* pSelectionMarker1 = &pTL->cursor;
    if (pTL->pRuns[pSelectionMarker0->iRun].iChar + pSelectionMarker0->iChar > pTL->pRuns[pSelectionMarker1->iRun].iChar + pSelectionMarker1->iChar)
    {
        drgui_text_marker* temp = pSelectionMarker0;
        pSelectionMarker0 = pSelectionMarker1;
        pSelectionMarker1 = temp;
    }

    unsigned int iSelectionChar0 = pTL->pRuns[pSelectionMarker0->iRun].iChar + pSelectionMarker0->iChar;
    unsigned int iSelectionChar1 = pTL->pRuns[pSelectionMarker1->iRun].iChar + pSelectionMarker1->iChar;

    bool wasTextChanged = drgui_text_layout_delete_text_range(pTL, iSelectionChar0, iSelectionChar1);
    if (wasTextChanged)
    {
        // The marker needs to be updated based on the new layout.
        drgui_text_layout__move_marker_to_character(pTL, &pTL->cursor, iSelectionChar0);

        // The cursor's sticky position also needs to be updated.
        drgui_text_layout__update_marker_sticky_position(pTL, &pTL->cursor);

        drgui_text_layout__on_cursor_move(pTL);


        // Reset the selection marker.
        pTL->selectionAnchor = pTL->cursor;
        pTL->isAnythingSelected = false;
    }

    return wasTextChanged;
}


void drgui_text_layout_enter_selection_mode(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return;
    }

    // If we've just entered selection mode and nothing is currently selected, we want to set the selection anchor to the current cursor position.
    if (!drgui_text_layout_is_in_selection_mode(pTL) && !pTL->isAnythingSelected) {
        pTL->selectionAnchor = pTL->cursor;
    }

    pTL->selectionModeCounter += 1;
}

void drgui_text_layout_leave_selection_mode(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return;
    }

    if (pTL->selectionModeCounter > 0) {
        pTL->selectionModeCounter -= 1;
    }
}

bool drgui_text_layout_is_in_selection_mode(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return false;
    }

    return pTL->selectionModeCounter > 0;
}

bool drgui_text_layout_is_anything_selected(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return false;
    }

    return pTL->isAnythingSelected;
}

void drgui_text_layout_deselect_all(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return;
    }

    pTL->isAnythingSelected = false;

    if (pTL->onDirty) {
        pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
    }
}

void drgui_text_layout_select_all(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return;
    }

    drgui_text_layout__move_marker_to_start_of_text(pTL, &pTL->selectionAnchor);
    drgui_text_layout__move_marker_to_end_of_text(pTL, &pTL->cursor);

    pTL->isAnythingSelected = drgui_text_layout__has_spacing_between_selection_markers(pTL);

    drgui_text_layout__on_cursor_move(pTL);

    if (pTL->onDirty) {
        pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
    }
}

void drgui_text_layout_select(drgui_text_layout* pTL, unsigned int firstCharacter, unsigned int lastCharacter)
{
    if (pTL == NULL) {
        return;
    }

    drgui_text_layout__move_marker_to_character(pTL, &pTL->selectionAnchor, firstCharacter);
    drgui_text_layout__move_marker_to_character(pTL, &pTL->cursor, lastCharacter);

    pTL->isAnythingSelected = drgui_text_layout__has_spacing_between_selection_markers(pTL);

    drgui_text_layout__on_cursor_move(pTL);

    if (pTL->onDirty) {
        pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
    }
}

size_t drgui_text_layout_get_selected_text(drgui_text_layout* pTL, char* textOut, size_t textOutSize)
{
    if (pTL == NULL || (textOut != NULL && textOutSize == 0)) {
        return 0;
    }

    if (!drgui_text_layout_is_anything_selected(pTL)) {
        return 0;
    }


    drgui_text_marker* pSelectionMarker0;
    drgui_text_marker* pSelectionMarker1;
    if (!drgui_text_layout__get_selection_markers(pTL, &pSelectionMarker0, &pSelectionMarker1)) {
        return false;
    }

    unsigned int iSelectionChar0 = pTL->pRuns[pSelectionMarker0->iRun].iChar + pSelectionMarker0->iChar;
    unsigned int iSelectionChar1 = pTL->pRuns[pSelectionMarker1->iRun].iChar + pSelectionMarker1->iChar;

    size_t selectedTextLength = iSelectionChar1 - iSelectionChar0;

    if (textOut != NULL) {
        drgui__strncpy_s(textOut, textOutSize, pTL->text + iSelectionChar0, selectedTextLength);
    }

    return selectedTextLength;
}

unsigned int drgui_text_layout_get_selection_first_line(drgui_text_layout* pTL)
{
    if (pTL == NULL || pTL->runCount == 0) {
        return 0;
    }

    drgui_text_marker* pSelectionMarker0;
    drgui_text_marker* pSelectionMarker1;
    if (!drgui_text_layout__get_selection_markers(pTL, &pSelectionMarker0, &pSelectionMarker1)) {
        return 0;
    }

    return pTL->pRuns[pSelectionMarker0->iRun].iLine;
}

unsigned int drgui_text_layout_get_selection_last_line(drgui_text_layout* pTL)
{
    if (pTL == NULL || pTL->runCount == 0) {
        return 0;
    }

    drgui_text_marker* pSelectionMarker0;
    drgui_text_marker* pSelectionMarker1;
    if (!drgui_text_layout__get_selection_markers(pTL, &pSelectionMarker0, &pSelectionMarker1)) {
        return 0;
    }

    return pTL->pRuns[pSelectionMarker1->iRun].iLine;
}

void drgui_text_layout_move_selection_anchor_to_end_of_line(drgui_text_layout* pTL, unsigned int iLine)
{
    if (pTL == NULL) {
        return;
    }

    drgui_text_layout__move_marker_to_end_of_line_by_index(pTL, &pTL->selectionAnchor, iLine);
    pTL->isAnythingSelected = drgui_text_layout__has_spacing_between_selection_markers(pTL);
}

void drgui_text_layout_move_selection_anchor_to_start_of_line(drgui_text_layout* pTL, unsigned int iLine)
{
    if (pTL == NULL) {
        return;
    }

    drgui_text_layout__move_marker_to_start_of_line_by_index(pTL, &pTL->selectionAnchor, iLine);
    pTL->isAnythingSelected = drgui_text_layout__has_spacing_between_selection_markers(pTL);
}

unsigned int drgui_text_layout_get_selection_anchor_line(drgui_text_layout* pTL)
{
    if (pTL == NULL || pTL->runCount == 0) {
        return 0;
    }

    return pTL->pRuns[pTL->selectionAnchor.iRun].iLine;
}



bool drgui_text_layout_prepare_undo_point(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return false;
    }

    // If we have a previously prepared state we'll need to clear it.
    if (pTL->preparedState.text != NULL) {
        free(pTL->preparedState.text);
    }

    pTL->preparedState.text = malloc(pTL->textLength + 1);
    drgui__strcpy_s(pTL->preparedState.text, pTL->textLength + 1, (pTL->text != NULL) ? pTL->text : "");

    pTL->preparedState.cursorPos          = drgui_text_layout__get_marker_absolute_char_index(pTL, &pTL->cursor);
    pTL->preparedState.selectionAnchorPos = drgui_text_layout__get_marker_absolute_char_index(pTL, &pTL->selectionAnchor);
    pTL->preparedState.isAnythingSelected = pTL->isAnythingSelected;

    return true;
}

bool drgui_text_layout_commit_undo_point(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return false;
    }

    // The undo point must have been prepared earlier.
    if (pTL->preparedState.text == NULL) {
        return false;
    }


    // The undo state is creating by diff-ing the prepared state and the current state.
    drgui_text_layout_state currentState;
    currentState.text               = pTL->text;
    currentState.cursorPos          = drgui_text_layout__get_marker_absolute_char_index(pTL, &pTL->cursor);
    currentState.selectionAnchorPos = drgui_text_layout__get_marker_absolute_char_index(pTL, &pTL->selectionAnchor);
    currentState.isAnythingSelected = pTL->isAnythingSelected;

    drgui_text_layout_undo_state undoState;
    if (!drgui_text_layout__diff_states(&pTL->preparedState, &currentState, &undoState)) {
        return false;
    }


    // At this point we have the undo state ready and we just need to add it the undo stack. Before doing so, however,
    // we need to trim the end fo the stack.
    drgui_text_layout__trim_undo_stack(pTL);
    drgui_text_layout__push_undo_state(pTL, &undoState);

    return true;
}

bool drgui_text_layout_undo(drgui_text_layout* pTL)
{
    if (pTL == NULL || pTL->pUndoStack == NULL) {
        return false;
    }

    if (drgui_text_layout_get_undo_points_remaining_count(pTL) > 0)
    {
        drgui_text_layout_undo_state* pUndoState = pTL->pUndoStack + (pTL->iUndoState - 1);
        assert(pUndoState != NULL);

        drgui_text_layout__apply_undo_state(pTL, pUndoState);
        pTL->iUndoState -= 1;

        if (pTL->onUndoPointChanged) {
            pTL->onUndoPointChanged(pTL, pTL->iUndoState);
        }

        return true;
    }

    return false;
}

bool drgui_text_layout_redo(drgui_text_layout* pTL)
{
    if (pTL == NULL || pTL->pUndoStack == NULL) {
        return false;
    }

    if (drgui_text_layout_get_redo_points_remaining_count(pTL) > 0)
    {
        drgui_text_layout_undo_state* pUndoState = pTL->pUndoStack + pTL->iUndoState;
        assert(pUndoState != NULL);

        drgui_text_layout__apply_redo_state(pTL, pUndoState);
        pTL->iUndoState += 1;

        if (pTL->onUndoPointChanged) {
            pTL->onUndoPointChanged(pTL, pTL->iUndoState);
        }

        return true;
    }

    return false;
}

unsigned int drgui_text_layout_get_undo_points_remaining_count(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return 0;
    }

    return pTL->iUndoState;
}

unsigned int drgui_text_layout_get_redo_points_remaining_count(drgui_text_layout* pTL)
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

void drgui_text_layout_clear_undo_stack(drgui_text_layout* pTL)
{
    if (pTL == NULL || pTL->pUndoStack == NULL) {
        return;
    }

    for (unsigned int i = 0; i < pTL->undoStackCount; ++i) {
        drgui_text_layout__uninit_undo_state(pTL->pUndoStack + i);
    }

    free(pTL->pUndoStack);

    pTL->pUndoStack = NULL;
    pTL->undoStackCount = 0;

    if (pTL->iUndoState > 0) {
        pTL->iUndoState = 0;

        if (pTL->onUndoPointChanged) {
            pTL->onUndoPointChanged(pTL, pTL->iUndoState);
        }
    }
}



unsigned int drgui_text_layout_get_line_count(drgui_text_layout* pTL)
{
    if (pTL == NULL || pTL->runCount == 0) {
        return 0;
    }

    return pTL->pRuns[pTL->runCount - 1].iLine + 1;
}

unsigned int drgui_text_layout_get_visible_line_count_starting_at(drgui_text_layout* pTL, unsigned int iFirstLine)
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
    drgui_text_layout_line line;
    if (drgui_text_layout__first_line(pTL, &line))
    {
        do
        {
            if (iLine >= iFirstLine) {
                break;
            }

            iLine += 1;
        } while (drgui_text_layout__next_line(pTL, &line));


        // At this point we are at the first line and we need to start counting.
        do
        {
            if (line.posY + pTL->innerOffsetY >= pTL->containerHeight) {
                break;
            }

            count += 1;
            lastLineBottom = line.posY + line.height;

        } while (drgui_text_layout__next_line(pTL, &line));
    }


    // At this point there may be some empty space below the last line, in which case we use the line height of the default font to fill
    // out the remaining space.
    if (lastLineBottom + pTL->innerOffsetY < pTL->containerHeight)
    {
        drgui_font_metrics defaultFontMetrics;
        if (drgui_get_font_metrics(pTL->pDefaultFont, scaleX, scaleY, &defaultFontMetrics))
        {
            count += (unsigned int)((pTL->containerHeight - (lastLineBottom + pTL->innerOffsetY)) / defaultFontMetrics.lineHeight);
        }
    }



    if (count == 0) {
        return 1;
    }

    return count;
}

float drgui_text_layout_get_line_pos_y(drgui_text_layout* pTL, unsigned int iLine)
{
    drgui_rect lineRect;
    if (!drgui_text_layout__find_line_info_by_index(pTL, iLine, &lineRect, NULL, NULL)) {
        return 0;
    }

    return lineRect.top;
}

unsigned int drgui_text_layout_get_line_at_pos_y(drgui_text_layout* pTL, float posY)
{
    if (pTL == NULL || pTL->runCount == 0) {
        return 0;
    }

    drgui_rect textRect = drgui_text_layout_get_text_rect_relative_to_bounds(pTL);

    unsigned int iRun;

    float inputPosYRelativeToText = posY - textRect.top;
    if (!drgui_text_layout__find_closest_run_to_point(pTL, 0, inputPosYRelativeToText, &iRun)) {
        return 0;
    }

    return pTL->pRuns[iRun].iLine;
}

unsigned int drgui_text_layout_get_line_first_character(drgui_text_layout* pTL, unsigned int iLine)
{
    if (pTL == NULL || pTL->runCount == 0) {
        return 0;
    }

    unsigned int firstRunIndex0;
    unsigned int lastRunIndexPlus1;
    if (drgui_text_layout__find_line_info_by_index(pTL, iLine, NULL, &firstRunIndex0, &lastRunIndexPlus1)) {
        return pTL->pRuns[firstRunIndex0].iChar;
    }

    return 0;
}

unsigned int drgui_text_layout_get_line_last_character(drgui_text_layout* pTL, unsigned int iLine)
{
    if (pTL == NULL || pTL->runCount == 0) {
        return 0;
    }

    unsigned int firstRunIndex0;
    unsigned int lastRunIndexPlus1;
    if (drgui_text_layout__find_line_info_by_index(pTL, iLine, NULL, &firstRunIndex0, &lastRunIndexPlus1)) {
        unsigned int charEnd = pTL->pRuns[lastRunIndexPlus1 - 1].iCharEnd;
        if (charEnd > 0) {
            charEnd -= 1;
        }

        return charEnd;
    }

    return 0;
}

void drgui_text_layout_get_line_character_range(drgui_text_layout* pTL, unsigned int iLine, unsigned int* pCharStartOut, unsigned int* pCharEndOut)
{
    if (pTL == NULL || pTL->runCount == 0) {
        return;
    }

    unsigned int charStart = 0;
    unsigned int charEnd = 0;

    unsigned int firstRunIndex0;
    unsigned int lastRunIndexPlus1;
    if (drgui_text_layout__find_line_info_by_index(pTL, iLine, NULL, &firstRunIndex0, &lastRunIndexPlus1)) {
        charStart = pTL->pRuns[firstRunIndex0].iChar;
        charEnd   = pTL->pRuns[lastRunIndexPlus1 - 1].iCharEnd;
        if (charEnd > 0) {
            charEnd -= 1;
        }
    }

    if (pCharStartOut) {
        *pCharStartOut = charStart;
    }
    if (pCharEndOut) {
        *pCharEndOut = charEnd;
    }
}


void drgui_text_layout_set_on_paint_text(drgui_text_layout* pTL, drgui_text_layout_on_paint_text_proc proc)
{
    if (pTL == NULL) {
        return;
    }

    pTL->onPaintText = proc;
}

void drgui_text_layout_set_on_paint_rect(drgui_text_layout* pTL, drgui_text_layout_on_paint_rect_proc proc)
{
    if (pTL == NULL) {
        return;
    }

    pTL->onPaintRect = proc;
}

void drgui_text_layout_paint(drgui_text_layout* pTL, drgui_rect rect, drgui_element* pElement, void* pPaintData)
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
    drgui_rect textRect = drgui_text_layout_get_text_rect_relative_to_bounds(pTL);

    // We draw a rectangle above and below the text rectangle. The main text rectangle will be drawn by iterating over each visible run.
    drgui_rect rectTop    = drgui_make_rect(0, 0, pTL->containerWidth, textRect.top);
    drgui_rect rectBottom = drgui_make_rect(0, textRect.bottom, pTL->containerWidth, pTL->containerHeight);

    if (pTL->onPaintRect)
    {
        if (rectTop.bottom > rect.top) {
            pTL->onPaintRect(pTL, rectTop, pTL->defaultBackgroundColor, pElement, pPaintData);
        }

        if (rectBottom.top < rect.bottom) {
            pTL->onPaintRect(pTL, rectBottom, pTL->defaultBackgroundColor, pElement, pPaintData);
        }
    }


    // We draw line-by-line, starting from the first visible line.
    drgui_text_layout_line line;
    if (drgui_text_layout__first_line(pTL, &line))
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

                    drgui_color bgcolor = pTL->defaultBackgroundColor;
                    if (line.index == drgui_text_layout_get_cursor_line(pTL)) {
                        bgcolor = pTL->lineBackgroundColor;
                    }

                    float lineSelectionOverhangLeft  = 0;
                    float lineSelectionOverhangRight = 0;

                    if (drgui_text_layout_is_anything_selected(pTL))
                    {
                        drgui_text_marker* pSelectionMarker0 = &pTL->selectionAnchor;
                        drgui_text_marker* pSelectionMarker1 = &pTL->cursor;
                        if (pTL->pRuns[pSelectionMarker0->iRun].iChar + pSelectionMarker0->iChar > pTL->pRuns[pSelectionMarker1->iRun].iChar + pSelectionMarker1->iChar)
                        {
                            drgui_text_marker* temp = pSelectionMarker0;
                            pSelectionMarker0 = pSelectionMarker1;
                            pSelectionMarker1 = temp;
                        }

                        unsigned int iSelectionLine0 = pTL->pRuns[pSelectionMarker0->iRun].iLine;
                        unsigned int iSelectionLine1 = pTL->pRuns[pSelectionMarker1->iRun].iLine;

                        if (line.index >= iSelectionLine0 && line.index < iSelectionLine1)
                        {
                            drgui_font_metrics defaultFontMetrics;
                            drgui_get_font_metrics(pTL->pDefaultFont, scaleX, scaleY, &defaultFontMetrics);

                            if (pTL->horzAlign == drgui_text_layout_alignment_right)
                            {
                                if (line.index > iSelectionLine0) {
                                    lineSelectionOverhangLeft = (float)defaultFontMetrics.spaceWidth;
                                }
                            }
                            else if (pTL->horzAlign == drgui_text_layout_alignment_center)
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


                    drgui_text_run* pFirstRun = pTL->pRuns + line.iFirstRun;
                    drgui_text_run* pLastRun  = pTL->pRuns + line.iLastRun;

                    float lineLeft  = pFirstRun->posX + textRect.left;
                    float lineRight = pLastRun->posX + pLastRun->width + textRect.left;

                    // 1) The blank space to the left of the first run.
                    if (lineLeft > 0)
                    {
                        if (lineSelectionOverhangLeft > 0) {
                            pTL->onPaintRect(pTL, drgui_make_rect(lineLeft - lineSelectionOverhangLeft, lineTop, lineLeft, lineBottom), pTL->selectionBackgroundColor, pElement, pPaintData);
                        }

                        pTL->onPaintRect(pTL, drgui_make_rect(0, lineTop, lineLeft - lineSelectionOverhangLeft, lineBottom), bgcolor, pElement, pPaintData);
                    }


                    // 2) The runs themselves.
                    for (unsigned int iRun = line.iFirstRun; iRun <= line.iLastRun; ++iRun)
                    {
                        drgui_text_run* pRun = pTL->pRuns + iRun;

                        float runLeft  = pRun->posX + textRect.left;
                        float runRight = runLeft    + pRun->width;

                        if (runRight > 0 && runLeft < pTL->containerWidth)
                        {
                            // The run is visible.
                            if (!drgui_text_layout__is_text_run_whitespace(pTL, pRun) || pTL->text[pRun->iChar] == '\t')
                            {
                                drgui_text_run run = pTL->pRuns[iRun];
                                run.pFont           = pTL->pDefaultFont;
                                run.textColor       = pTL->defaultTextColor;
                                run.backgroundColor = bgcolor;
                                run.text            = pTL->text + run.iChar;
                                run.posX            = runLeft;
                                run.posY            = lineTop;

                                // We paint the run differently depending on whether or not anything is selected. If something is selected
                                // we need to split the run into a maximum of 3 sub-runs so that the selection rectangle can be drawn correctly.
                                if (drgui_text_layout_is_anything_selected(pTL))
                                {
                                    drgui_text_run subruns[3];
                                    unsigned int subrunCount = drgui_text_layout__split_text_run_by_selection(pTL, &run, subruns);
                                    for (unsigned int iSubRun = 0; iSubRun < subrunCount; ++iSubRun)
                                    {
                                        drgui_text_run* pSubRun = subruns + iSubRun;

                                        if (!drgui_text_layout__is_text_run_whitespace(pTL, pRun)) {
                                            pTL->onPaintText(pTL, pSubRun, pElement, pPaintData);
                                        } else {
                                            pTL->onPaintRect(pTL, drgui_make_rect(pSubRun->posX, lineTop, pSubRun->posX + pSubRun->width, lineBottom), pSubRun->backgroundColor, pElement, pPaintData);
                                        }
                                    }
                                }
                                else
                                {
                                    // Nothing is selected.
                                    if (!drgui_text_layout__is_text_run_whitespace(pTL, &run)) {
                                        pTL->onPaintText(pTL, &run, pElement, pPaintData);
                                    } else {
                                        pTL->onPaintRect(pTL, drgui_make_rect(run.posX, lineTop, run.posX + run.width, lineBottom), run.backgroundColor, pElement, pPaintData);
                                    }
                                }
                            }
                        }
                    }


                    // 3) The blank space to the right of the last run.
                    if (lineRight < pTL->containerWidth)
                    {
                        if (lineSelectionOverhangRight > 0) {
                            pTL->onPaintRect(pTL, drgui_make_rect(lineRight, lineTop, lineRight + lineSelectionOverhangRight, lineBottom), pTL->selectionBackgroundColor, pElement, pPaintData);
                        }

                        pTL->onPaintRect(pTL, drgui_make_rect(lineRight + lineSelectionOverhangRight, lineTop, pTL->containerWidth, lineBottom), bgcolor, pElement, pPaintData);
                    }
                }
            }
            else
            {
                // The line is below the rectangle which means no other line will be visible and we can terminate early.
                break;
            }

        } while (drgui_text_layout__next_line(pTL, &line));
    }
    else
    {
        // There are no lines so we do a simplified branch here.
        float lineTop    = textRect.top;
        float lineBottom = textRect.bottom;
        pTL->onPaintRect(pTL, drgui_make_rect(0, lineTop, pTL->containerWidth, lineBottom), pTL->lineBackgroundColor, pElement, pPaintData);
    }

    // The cursor.
    if (pTL->isShowingCursor && pTL->isCursorBlinkOn) {
        pTL->onPaintRect(pTL, drgui_text_layout_get_cursor_rect(pTL), pTL->cursorColor, pElement, pPaintData);
    }
}


void drgui_text_layout_step(drgui_text_layout* pTL, unsigned int milliseconds)
{
    if (pTL == NULL || milliseconds == 0) {
        return;
    }

    if (pTL->timeToNextCursorBlink < milliseconds)
    {
        pTL->isCursorBlinkOn = !pTL->isCursorBlinkOn;
        pTL->timeToNextCursorBlink = pTL->cursorBlinkRate;

        if (pTL->onDirty) {
            pTL->onDirty(pTL, drgui_text_layout_get_cursor_rect(pTL));
        }
    }
    else
    {
        pTL->timeToNextCursorBlink -= milliseconds;
    }
}



void drgui_text_layout_paint_line_numbers(drgui_text_layout* pTL, float lineNumbersWidth, float lineNumbersHeight, drgui_color textColor, drgui_text_layout_on_paint_text_proc onPaintText, drgui_text_layout_on_paint_rect_proc onPaintRect, drgui_element* pElement, void* pPaintData)
{
    if (pTL == NULL || onPaintText == NULL || onPaintRect == NULL) {
        return;
    }

    float scaleX = 1;
    float scaleY = 1;


    // The position of each run will be relative to the text bounds. We want to make it relative to the container bounds.
    drgui_rect textRect = drgui_text_layout_get_text_rect_relative_to_bounds(pTL);

    // We draw a rectangle above and below the text rectangle. The main text rectangle will be drawn by iterating over each visible run.
    drgui_rect rectTop    = drgui_make_rect(0, 0, lineNumbersWidth, textRect.top);
    drgui_rect rectBottom = drgui_make_rect(0, textRect.bottom, lineNumbersWidth, lineNumbersHeight);

    if (pTL->onPaintRect)
    {
        if (rectTop.bottom > 0) {
            onPaintRect(pTL, rectTop, pTL->defaultBackgroundColor, pElement, pPaintData);
        }

        if (rectBottom.top < lineNumbersHeight) {
            onPaintRect(pTL, rectBottom, pTL->defaultBackgroundColor, pElement, pPaintData);
        }
    }


    // Now we draw each line.
    int iLine = 1;
    drgui_text_layout_line line;
    if (!drgui_text_layout__first_line(pTL, &line))
    {
        // We failed to retrieve the first line which is probably due to the text layout being empty. We just fake the first line to
        // ensure we get the number 1 to be drawn.
        drgui_font_metrics fontMetrics;
        drgui_get_font_metrics(pTL->pDefaultFont, scaleX, scaleY, &fontMetrics);

        line.height = (float)fontMetrics.lineHeight;
        line.posY = 0;
    }

    do
    {
        float lineTop    = line.posY + textRect.top;
        float lineBottom = lineTop + line.height;

        if (lineTop < lineNumbersHeight)
        {
            if (lineBottom > 0)
            {
                char iLineStr[64];
                #ifdef _MSC_VER
                _itoa_s(iLine, iLineStr, sizeof(iLineStr), 10);
                #else
                snprintf(iLineStr, sizeof(iLineStr), "%d", iLine);
                #endif

                drgui_font* pFont = pTL->pDefaultFont;

                float textWidth;
                float textHeight;
                drgui_measure_string(pFont, iLineStr, strlen(iLineStr), scaleX, scaleY, &textWidth, &textHeight);

                drgui_text_run run = {0};
                run.pFont           = pFont;
                run.textColor       = textColor;
                run.backgroundColor = pTL->defaultBackgroundColor;
                run.text            = iLineStr;
                run.textLength      = strlen(iLineStr);
                run.posX            = lineNumbersWidth - textWidth;
                run.posY            = lineTop;
                onPaintText(pTL, &run, pElement, pPaintData);
                onPaintRect(pTL, drgui_make_rect(0, lineTop, run.posX, lineBottom), run.backgroundColor, pElement, pPaintData);
            }
        }
        else
        {
            // The line is below the rectangle which means no other line will be visible and we can terminate early.
            break;
        }

        iLine += 1;
    } while (drgui_text_layout__next_line(pTL, &line));
}


bool drgui_text_layout_find_next(drgui_text_layout* pTL, const char* text, unsigned int* pSelectionStartOut, unsigned int* pSelectionEndOut)
{
    if (pTL == NULL || text == NULL || text[0] == '\0') {
        return false;
    }

    unsigned int cursorPos = drgui_text_layout__get_marker_absolute_char_index(pTL, &pTL->cursor);
    char* nextOccurance = strstr(pTL->text + cursorPos, text);
    if (nextOccurance == NULL) {
        nextOccurance = strstr(pTL->text, text);
    }

    if (nextOccurance == NULL) {
        return false;
    }

    if (pSelectionStartOut) {
        *pSelectionStartOut = (unsigned int)(nextOccurance - pTL->text);
    }
    if (pSelectionEndOut) {
        *pSelectionEndOut = (unsigned int)(nextOccurance - pTL->text) + strlen(text);
    }

    return true;
}

bool drgui_text_layout_find_next_no_loop(drgui_text_layout* pTL, const char* text, unsigned int* pSelectionStartOut, unsigned int* pSelectionEndOut)
{
    if (pTL == NULL || text == NULL || text[0] == '\0') {
        return false;
    }

    unsigned int cursorPos = drgui_text_layout__get_marker_absolute_char_index(pTL, &pTL->cursor);

    char* nextOccurance = strstr(pTL->text + cursorPos, text);
    if (nextOccurance == NULL) {
        return false;
    }

    if (pSelectionStartOut) {
        *pSelectionStartOut = (unsigned int)(nextOccurance - pTL->text);
    }
    if (pSelectionEndOut) {
        *pSelectionEndOut = (unsigned int)(nextOccurance - pTL->text) + strlen(text);
    }

    return true;
}




DRGUI_PRIVATE bool drgui_next_run_string(const char* runStart, const char* textEndPastNullTerminator, const char** pRunEndOut)
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

DRGUI_PRIVATE void drgui_text_layout__refresh(drgui_text_layout* pTL)
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
    drgui_text_layout__clear_text_runs(pTL);

    // The text bounds also need to be reset at the top.
    pTL->textBoundsWidth  = 0;
    pTL->textBoundsHeight = 0;

    const float scaleX = 1;
    const float scaleY = 1;

    drgui_font_metrics defaultFontMetrics;
    drgui_get_font_metrics(pTL->pDefaultFont, scaleX, scaleY, &defaultFontMetrics);

    pTL->textBoundsHeight = (float)defaultFontMetrics.lineHeight;

    const float tabWidth = drgui_text_layout__get_tab_width(pTL);

    unsigned int iCurrentLine  = 0;
    float runningPosY       = 0;
    float runningLineHeight = 0;

    const char* nextRunStart = pTL->text;
    const char* nextRunEnd;
    while (drgui_next_run_string(nextRunStart, pTL->text + pTL->textLength + 1, OUT &nextRunEnd))
    {
        drgui_text_run run;
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
            drgui_text_run* pPrevRun = pTL->pRuns + (pTL->runCount - 1);
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
            drgui_measure_string(pTL->pDefaultFont, nextRunStart, run.textLength, 1, 1, &run.width, &run.height);
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
        drgui_text_layout__push_text_run(pTL, &run);

        // Go to the next run string.
        nextRunStart = nextRunEnd;
    }

    // If we were to return now the text would be alignment top/left. If the alignment is not top/left we need to refresh the layout.
    if (pTL->horzAlign != drgui_text_layout_alignment_left || pTL->vertAlign != drgui_text_layout_alignment_top)
    {
        drgui_text_layout__refresh_alignment(pTL);
    }
}

DRGUI_PRIVATE void drgui_text_layout__refresh_alignment(drgui_text_layout* pTL)
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
            drgui_text_run* pRun = pTL->pRuns + jRun;
            pRun->posX = lineWidth;
            pRun->posY = runningPosY;

            lineWidth += pRun->width;
            lineHeight = (lineHeight > pRun->height) ? lineHeight : pRun->height;
        }


        // The actual alignment is done here.
        float lineOffsetX;
        float lineOffsetY;
        drgui_text_layout__calculate_line_alignment_offset(pTL, lineWidth, OUT &lineOffsetX, OUT &lineOffsetY);

        for (DO_NOTHING; iRun < jRun; ++iRun)
        {
            drgui_text_run* pRun = pTL->pRuns + iRun;
            pRun->posX += lineOffsetX;
            pRun->posY += lineOffsetY;
        }


        // Go to the next line.
        iCurrentLine += 1;
        runningPosY  += lineHeight;
    }
}

void drgui_text_layout__calculate_line_alignment_offset(drgui_text_layout* pTL, float lineWidth, float* pOffsetXOut, float* pOffsetYOut)
{
    if (pTL == NULL) {
        return;
    }

    float offsetX = 0;
    float offsetY = 0;

    switch (pTL->horzAlign)
    {
    case drgui_text_layout_alignment_right:
        {
            offsetX = pTL->textBoundsWidth - lineWidth;
            break;
        }

    case drgui_text_layout_alignment_center:
        {
            offsetX = (pTL->textBoundsWidth - lineWidth) / 2;
            break;
        }

    case drgui_text_layout_alignment_left:
    case drgui_text_layout_alignment_top:     // Invalid for horizontal alignment.
    case drgui_text_layout_alignment_bottom:  // Invalid for horizontal alignment.
    default:
        {
            offsetX = 0;
            break;
        }
    }


    switch (pTL->vertAlign)
    {
    case drgui_text_layout_alignment_bottom:
        {
            offsetY = pTL->textBoundsHeight - pTL->textBoundsHeight;
            break;
        }

    case drgui_text_layout_alignment_center:
        {
            offsetY = (pTL->textBoundsHeight - pTL->textBoundsHeight) / 2;
            break;
        }

    case drgui_text_layout_alignment_top:
    case drgui_text_layout_alignment_left:    // Invalid for vertical alignment.
    case drgui_text_layout_alignment_right:   // Invalid for vertical alignment.
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


DRGUI_PRIVATE void drgui_text_layout__push_text_run(drgui_text_layout* pTL, drgui_text_run* pRun)
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

        pTL->pRuns = realloc(pTL->pRuns, sizeof(drgui_text_run) * pTL->runBufferSize);
        if (pTL->pRuns == NULL) {
            pTL->runCount = 0;
            pTL->runBufferSize = 0;
            return;
        }
    }

    pTL->pRuns[pTL->runCount] = *pRun;
    pTL->runCount += 1;
}

DRGUI_PRIVATE void drgui_text_layout__clear_text_runs(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return;
    }

    pTL->runCount = 0;
}

DRGUI_PRIVATE bool drgui_text_layout__is_text_run_whitespace(drgui_text_layout* pTL, drgui_text_run* pRun)
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

DRGUI_PRIVATE float drgui_text_layout__get_tab_width(drgui_text_layout* pTL)
{
    drgui_font_metrics defaultFontMetrics;
    drgui_get_font_metrics(pTL->pDefaultFont, 1, 1, &defaultFontMetrics);

    return (float)(defaultFontMetrics.spaceWidth * pTL->tabSizeInSpaces);
}


DRGUI_PRIVATE bool drgui_text_layout__find_closest_line_to_point(drgui_text_layout* pTL, float inputPosYRelativeToText, unsigned int* pFirstRunIndexOnLineOut, unsigned int* pLastRunIndexOnLinePlus1Out)
{
    unsigned int iFirstRunOnLine     = 0;
    unsigned int iLastRunOnLinePlus1 = 0;

    bool result = true;
    if (pTL == NULL || pTL->runCount == 0)
    {
        result = false;
    }
    else
    {
        float runningLineTop = 0;

        float lineHeight;
        while (drgui_text_layout__find_line_info(pTL, iFirstRunOnLine, OUT &iLastRunOnLinePlus1, OUT &lineHeight))
        {
            const float lineTop    = runningLineTop;
            const float lineBottom = lineTop + lineHeight;

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

        if (iFirstRunOnLine == iLastRunOnLinePlus1 && iFirstRunOnLine > 0) {
            iFirstRunOnLine -= 1;
        }
    }

    if (pFirstRunIndexOnLineOut) {
        *pFirstRunIndexOnLineOut = iFirstRunOnLine;
    }
    if (pLastRunIndexOnLinePlus1Out) {
        *pLastRunIndexOnLinePlus1Out = iLastRunOnLinePlus1;
    }

    return result;
}

DRGUI_PRIVATE bool drgui_text_layout__find_closest_run_to_point(drgui_text_layout* pTL, float inputPosXRelativeToText, float inputPosYRelativeToText, unsigned int* pRunIndexOut)
{
    if (pTL == NULL) {
        return false;
    }

    unsigned int iFirstRunOnLine;
    unsigned int iLastRunOnLinePlus1;
    if (drgui_text_layout__find_closest_line_to_point(pTL, inputPosYRelativeToText, OUT &iFirstRunOnLine, OUT &iLastRunOnLinePlus1))
    {
        unsigned int iRunOut = 0;

        const drgui_text_run* pFirstRunOnLine = pTL->pRuns + iFirstRunOnLine;
        const drgui_text_run* pLastRunOnLine  = pTL->pRuns + (iLastRunOnLinePlus1 - 1);

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
                const drgui_text_run* pRun = pTL->pRuns + iRun;
                iRunOut = iRun;

                if (inputPosXRelativeToText >= pRun->posX && inputPosXRelativeToText <= pRun->posX + pRun->width) {
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

DRGUI_PRIVATE bool drgui_text_layout__find_line_info(drgui_text_layout* pTL, unsigned int iFirstRunOnLine, unsigned int* pLastRunIndexOnLinePlus1Out, float* pLineHeightOut)
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

DRGUI_PRIVATE bool drgui_text_layout__find_line_info_by_index(drgui_text_layout* pTL, unsigned int iLine, drgui_rect* pRectOut, unsigned int* pFirstRunIndexOut, unsigned int* pLastRunIndexPlus1Out)
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
        iFirstRunOnLine = iLastRunOnLinePlus1;
        lineTop += lineHeight;

        if (!drgui_text_layout__find_line_info(pTL, iFirstRunOnLine, &iLastRunOnLinePlus1, &lineHeight))
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

DRGUI_PRIVATE bool drgui_text_layout__find_last_run_on_line_starting_from_run(drgui_text_layout* pTL, unsigned int iRun, unsigned int* pLastRunIndexOnLineOut)
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

DRGUI_PRIVATE bool drgui_text_layout__find_first_run_on_line_starting_from_run(drgui_text_layout* pTL, unsigned int iRun, unsigned int* pFirstRunIndexOnLineOut)
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

DRGUI_PRIVATE bool drgui_text_layout__find_run_at_character(drgui_text_layout* pTL, unsigned int iChar, unsigned int* pRunIndexOut)
{
    if (pTL == NULL || pTL->runCount == 0) {
        return false;
    }

    unsigned int result = 0;
    if (iChar < pTL->textLength)
    {
        for (unsigned int iRun = 0; iRun < pTL->runCount; ++iRun)
        {
            const drgui_text_run* pRun = pTL->pRuns + iRun;

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


DRGUI_PRIVATE drgui_text_marker drgui_text_layout__new_marker()
{
    drgui_text_marker marker;
    marker.iRun              = 0;
    marker.iChar             = 0;
    marker.relativePosX      = 0;
    marker.absoluteSickyPosX = 0;

    return marker;
}

DRGUI_PRIVATE bool drgui_text_layout__move_marker_to_point_relative_to_container(drgui_text_layout* pTL, drgui_text_marker* pMarker, float inputPosX, float inputPosY)
{
    if (pTL == NULL || pMarker == NULL) {
        return false;
    }

    pMarker->iRun              = 0;
    pMarker->iChar             = 0;
    pMarker->relativePosX      = 0;
    pMarker->absoluteSickyPosX = 0;

    drgui_rect textRect = drgui_text_layout_get_text_rect_relative_to_bounds(pTL);

    float inputPosXRelativeToText = inputPosX - textRect.left;
    float inputPosYRelativeToText = inputPosY - textRect.top;
    if (drgui_text_layout__move_marker_to_point(pTL, pMarker, inputPosXRelativeToText, inputPosYRelativeToText))
    {
        drgui_text_layout__update_marker_sticky_position(pTL, pMarker);
        return true;
    }

    return false;
}

DRGUI_PRIVATE void drgui_text_layout__get_marker_position_relative_to_container(drgui_text_layout* pTL, drgui_text_marker* pMarker, float* pPosXOut, float* pPosYOut)
{
    if (pTL == NULL || pMarker == NULL) {
        return;
    }

    float posX = 0;
    float posY = 0;

    if (pMarker->iRun < pTL->runCount)
    {
        posX = pTL->pRuns[pMarker->iRun].posX + pMarker->relativePosX;
        posY = pTL->pRuns[pMarker->iRun].posY;
    }

    drgui_rect textRect = drgui_text_layout_get_text_rect_relative_to_bounds(pTL);
    posX += textRect.left;
    posY += textRect.top;


    if (pPosXOut) {
        *pPosXOut = posX;
    }
    if (pPosYOut) {
        *pPosYOut = posY;
    }
}

DRGUI_PRIVATE bool drgui_text_layout__move_marker_to_point(drgui_text_layout* pTL, drgui_text_marker* pMarker, float inputPosXRelativeToText, float inputPosYRelativeToText)
{
    if (pTL == NULL || pMarker == NULL) {
        return false;
    }

    const float scaleX = 1;
    const float scaleY = 1;

    unsigned int iClosestRunToPoint;
    if (drgui_text_layout__find_closest_run_to_point(pTL, inputPosXRelativeToText, inputPosYRelativeToText, OUT &iClosestRunToPoint))
    {
        const drgui_text_run* pRun = pTL->pRuns + iClosestRunToPoint;

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

                const float tabWidth = drgui_text_layout__get_tab_width(pTL);

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
                    drgui_text_layout__move_marker_to_first_character_of_next_run(pTL, pMarker);
                }
            }
            else
            {
                // It's a standard run.
                float inputPosXRelativeToRun = inputPosXRelativeToText - pRun->posX;
                if (drgui_get_text_cursor_position_from_point(pRun->pFont, pTL->text + pRun->iChar, pRun->textLength, pRun->width, inputPosXRelativeToRun, scaleX, scaleY, OUT &pMarker->relativePosX, OUT &pMarker->iChar))
                {
                    // If the marker is past the last character of the run it needs to be moved to the start of the next one.
                    if (pMarker->iChar == pRun->textLength) {
                        drgui_text_layout__move_marker_to_first_character_of_next_run(pTL, pMarker);
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
}

DRGUI_PRIVATE bool drgui_text_layout__move_marker_left(drgui_text_layout* pTL, drgui_text_marker* pMarker)
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

            const drgui_text_run* pRun = pTL->pRuns + pMarker->iRun;
            if (pTL->text[pRun->iChar] == '\t')
            {
                const float tabWidth = drgui_text_layout__get_tab_width(pTL);

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
                if (!drgui_get_text_cursor_position_from_char(pRun->pFont, pTL->text + pTL->pRuns[pMarker->iRun].iChar, pMarker->iChar, scaleX, scaleY, OUT &pMarker->relativePosX))
                {
                    return false;
                }
            }
        }
        else
        {
            // We're at the beginning of the run which means we need to transfer the cursor to the end of the previous run.
            if (!drgui_text_layout__move_marker_to_last_character_of_prev_run(pTL, pMarker))
            {
                return false;
            }
        }

        drgui_text_layout__update_marker_sticky_position(pTL, pMarker);
        return true;
    }

    return false;
}

DRGUI_PRIVATE bool drgui_text_layout__move_marker_right(drgui_text_layout* pTL, drgui_text_marker* pMarker)
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

            const drgui_text_run* pRun = pTL->pRuns + pMarker->iRun;
            if (pTL->text[pRun->iChar] == '\t')
            {
                const float tabWidth = drgui_text_layout__get_tab_width(pTL);

                pMarker->relativePosX  = tabWidth * ((pRun->posX + (tabWidth*(pMarker->iChar + 0))) / tabWidth);
                pMarker->relativePosX -= pRun->posX;
            }
            else
            {
                if (!drgui_get_text_cursor_position_from_char(pRun->pFont, pTL->text + pTL->pRuns[pMarker->iRun].iChar, pMarker->iChar, scaleX, scaleY, OUT &pMarker->relativePosX))
                {
                    return false;
                }
            }
        }
        else
        {
            // We're at the end of the run which means we need to transfer the cursor to the beginning of the next run.
            if (!drgui_text_layout__move_marker_to_first_character_of_next_run(pTL, pMarker))
            {
                return false;
            }
        }

        drgui_text_layout__update_marker_sticky_position(pTL, pMarker);
        return true;
    }

    return false;
}

DRGUI_PRIVATE bool drgui_text_layout__move_marker_up(drgui_text_layout* pTL, drgui_text_marker* pMarker)
{
    if (pTL == NULL || pMarker == NULL || pTL->runCount == 0) {
        return false;
    }

    return drgui_text_layout__move_marker_y(pTL, pMarker, -1);
}

DRGUI_PRIVATE bool drgui_text_layout__move_marker_down(drgui_text_layout* pTL, drgui_text_marker* pMarker)
{
    if (pTL == NULL || pMarker == NULL || pTL->runCount == 0) {
        return false;
    }

    return drgui_text_layout__move_marker_y(pTL, pMarker, 1);
}

DRGUI_PRIVATE bool drgui_text_layout__move_marker_y(drgui_text_layout* pTL, drgui_text_marker* pMarker, int amount)
{
    if (pTL == NULL || pMarker == NULL || pTL->runCount == 0) {
        return false;
    }

    const drgui_text_run* pOldRun = pTL->pRuns + pMarker->iRun;

    int iNewLine = (int)pOldRun->iLine + amount;
    if (iNewLine >= (int)drgui_text_layout_get_line_count(pTL)) {
        iNewLine = (int)drgui_text_layout_get_line_count(pTL) - 1;
    }
    if (iNewLine < 0) {
        iNewLine = 0;
    }

    if ((int)pOldRun->iLine == iNewLine) {
        return false;   // The lines are the same.
    }

    drgui_rect lineRect;
    unsigned int iFirstRunOnLine;
    unsigned int iLastRunOnLinePlus1;
    if (drgui_text_layout__find_line_info_by_index(pTL, (unsigned int)iNewLine, OUT &lineRect, OUT &iFirstRunOnLine, OUT &iLastRunOnLinePlus1))
    {
        float newMarkerPosX = pMarker->absoluteSickyPosX;
        float newMarkerPosY = lineRect.top;
        drgui_text_layout__move_marker_to_point(pTL, pMarker, newMarkerPosX, newMarkerPosY);

        return true;
    }
    else
    {
        // An error occured while finding information about the line above.
        return false;
    }
}

DRGUI_PRIVATE bool drgui_text_layout__move_marker_to_end_of_line(drgui_text_layout* pTL, drgui_text_marker* pMarker)
{
    if (pTL == NULL || pMarker == NULL) {
        return false;
    }

    unsigned int iLastRunOnLine;
    if (drgui_text_layout__find_last_run_on_line_starting_from_run(pTL, pMarker->iRun, &iLastRunOnLine))
    {
        return drgui_text_layout__move_marker_to_last_character_of_run(pTL, pMarker, iLastRunOnLine);
    }

    return false;
}

DRGUI_PRIVATE bool drgui_text_layout__move_marker_to_start_of_line(drgui_text_layout* pTL, drgui_text_marker* pMarker)
{
    if (pTL == NULL || pMarker == NULL) {
        return false;
    }

    unsigned int iFirstRunOnLine;
    if (drgui_text_layout__find_first_run_on_line_starting_from_run(pTL, pMarker->iRun, &iFirstRunOnLine))
    {
        return drgui_text_layout__move_marker_to_first_character_of_run(pTL, pMarker, iFirstRunOnLine);
    }

    return false;
}

DRGUI_PRIVATE bool drgui_text_layout__move_marker_to_end_of_line_by_index(drgui_text_layout* pTL, drgui_text_marker* pMarker, unsigned int iLine)
{
    if (pTL == NULL || pMarker == NULL) {
        return false;
    }

    unsigned int iFirstRun;
    unsigned int iLastRunPlus1;
    if (drgui_text_layout__find_line_info_by_index(pTL, iLine, NULL, &iFirstRun, &iLastRunPlus1))
    {
        return drgui_text_layout__move_marker_to_last_character_of_run(pTL, pMarker, iLastRunPlus1 - 1);
    }

    return false;
}

DRGUI_PRIVATE bool drgui_text_layout__move_marker_to_start_of_line_by_index(drgui_text_layout* pTL, drgui_text_marker* pMarker, unsigned int iLine)
{
    if (pTL == NULL || pMarker == NULL) {
        return false;
    }

    unsigned int iFirstRun;
    unsigned int iLastRunPlus1;
    if (drgui_text_layout__find_line_info_by_index(pTL, iLine, NULL, &iFirstRun, &iLastRunPlus1))
    {
        return drgui_text_layout__move_marker_to_first_character_of_run(pTL, pMarker, iFirstRun);
    }

    return false;
}

DRGUI_PRIVATE bool drgui_text_layout__move_marker_to_end_of_text(drgui_text_layout* pTL, drgui_text_marker* pMarker)
{
    if (pTL == NULL || pMarker == NULL) {
        return false;
    }

    if (pTL->runCount > 0) {
        return drgui_text_layout__move_marker_to_last_character_of_run(pTL, pMarker, pTL->runCount - 1);
    }

    return false;
}

DRGUI_PRIVATE bool drgui_text_layout__move_marker_to_start_of_text(drgui_text_layout* pTL, drgui_text_marker* pMarker)
{
    if (pTL == NULL || pMarker == NULL) {
        return false;
    }

    return drgui_text_layout__move_marker_to_first_character_of_run(pTL, pMarker, 0);
}

DRGUI_PRIVATE bool drgui_text_layout__move_marker_to_last_character_of_run(drgui_text_layout* pTL, drgui_text_marker* pMarker, unsigned int iRun)
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
            return drgui_text_layout__move_marker_left(pTL, pMarker);
        }

        return true;
    }

    return false;
}

DRGUI_PRIVATE bool drgui_text_layout__move_marker_to_first_character_of_run(drgui_text_layout* pTL, drgui_text_marker* pMarker, unsigned int iRun)
{
    if (pTL == NULL || pMarker == NULL) {
        return false;
    }

    if (iRun < pTL->runCount)
    {
        pMarker->iRun         = iRun;
        pMarker->iChar        = 0;
        pMarker->relativePosX = 0;

        drgui_text_layout__update_marker_sticky_position(pTL, pMarker);

        return true;
    }

    return false;
}

DRGUI_PRIVATE bool drgui_text_layout__move_marker_to_last_character_of_prev_run(drgui_text_layout* pTL, drgui_text_marker* pMarker)
{
    if (pTL == NULL || pMarker == NULL) {
        return false;
    }

    if (pMarker->iRun > 0) {
        return drgui_text_layout__move_marker_to_last_character_of_run(pTL, pMarker, pMarker->iRun - 1);
    }

    return false;
}

DRGUI_PRIVATE bool drgui_text_layout__move_marker_to_first_character_of_next_run(drgui_text_layout* pTL, drgui_text_marker* pMarker)
{
    if (pTL == NULL || pMarker == NULL) {
        return false;
    }

    if (pTL->runCount > 0 && pMarker->iRun < pTL->runCount - 1) {
        return drgui_text_layout__move_marker_to_first_character_of_run(pTL, pMarker, pMarker->iRun + 1);
    }

    return false;
}

DRGUI_PRIVATE bool drgui_text_layout__move_marker_to_character(drgui_text_layout* pTL, drgui_text_marker* pMarker, unsigned int iChar)
{
    if (pTL == NULL || pMarker == NULL || pTL->runCount == 0) {
        return false;
    }

    // Clamp the character to the end of the string.
    if (iChar > pTL->textLength) {
        iChar = pTL->textLength;
    }

    drgui_text_layout__find_run_at_character(pTL, iChar, &pMarker->iRun);

    assert(pMarker->iRun < pTL->runCount);
    pMarker->iChar = iChar - pTL->pRuns[pMarker->iRun].iChar;


    // The relative position depends on whether or not the run is a tab character.
    return drgui_text_layout__update_marker_relative_position(pTL, pMarker);
}


DRGUI_PRIVATE bool drgui_text_layout__update_marker_relative_position(drgui_text_layout* pTL, drgui_text_marker* pMarker)
{
    if (pTL == NULL || pMarker == NULL || pTL->runCount == 0) {
        return false;
    }

    float scaleX = 1;
    float scaleY = 1;

    const drgui_text_run* pRun = pTL->pRuns + pMarker->iRun;
    if (pTL->text[pRun->iChar] == '\t')
    {
        const float tabWidth = drgui_text_layout__get_tab_width(pTL);

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
        return drgui_get_text_cursor_position_from_char(pRun->pFont, pTL->text + pTL->pRuns[pMarker->iRun].iChar, pMarker->iChar, scaleX, scaleY, OUT &pMarker->relativePosX);
    }
}

DRGUI_PRIVATE void drgui_text_layout__update_marker_sticky_position(drgui_text_layout* pTL, drgui_text_marker* pMarker)
{
    if (pTL == NULL || pMarker == NULL) {
        return;
    }

    pMarker->absoluteSickyPosX = pTL->pRuns[pMarker->iRun].posX + pMarker->relativePosX;
}

DRGUI_PRIVATE unsigned int drgui_text_layout__get_marker_absolute_char_index(drgui_text_layout* pTL, drgui_text_marker* pMarker)
{
    if (pTL == NULL || pMarker == NULL || pTL->runCount == 0) {
        return 0;
    }

    return pTL->pRuns[pMarker->iRun].iChar + pMarker->iChar;
}


DRGUI_PRIVATE bool drgui_text_layout__has_spacing_between_selection_markers(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return false;
    }

    return (pTL->cursor.iRun != pTL->selectionAnchor.iRun || pTL->cursor.iChar != pTL->selectionAnchor.iChar);
}

DRGUI_PRIVATE unsigned int drgui_text_layout__split_text_run_by_selection(drgui_text_layout* pTL, drgui_text_run* pRunToSplit, drgui_text_run pSubRunsOut[3])
{
    if (pTL == NULL || pRunToSplit == NULL || pSubRunsOut == NULL) {
        return 0;
    }

    drgui_text_marker* pSelectionMarker0 = &pTL->selectionAnchor;
    drgui_text_marker* pSelectionMarker1 = &pTL->cursor;
    if (pTL->pRuns[pSelectionMarker0->iRun].iChar + pSelectionMarker0->iChar > pTL->pRuns[pSelectionMarker1->iRun].iChar + pSelectionMarker1->iChar)
    {
        drgui_text_marker* temp = pSelectionMarker0;
        pSelectionMarker0 = pSelectionMarker1;
        pSelectionMarker1 = temp;
    }

    drgui_text_run* pSelectionRun0 = pTL->pRuns + pSelectionMarker0->iRun;
    drgui_text_run* pSelectionRun1 = pTL->pRuns + pSelectionMarker1->iRun;

    unsigned int iSelectionChar0 = pSelectionRun0->iChar + pSelectionMarker0->iChar;
    unsigned int iSelectionChar1 = pSelectionRun1->iChar + pSelectionMarker1->iChar;

    if (drgui_text_layout_is_anything_selected(pTL))
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

#if 0
DRGUI_PRIVATE bool drgui_text_layout__is_run_selected(drgui_text_layout* pTL, unsigned int iRun)
{
    if (drgui_text_layout_is_anything_selected(pTL))
    {
        drgui_text_marker* pSelectionMarker0;
        drgui_text_marker* pSelectionMarker1;
        if (!drgui_text_layout__get_selection_markers(pTL, &pSelectionMarker0, &pSelectionMarker1)) {
            return false;
        }

        unsigned int iSelectionChar0 = pTL->pRuns[pSelectionMarker0->iRun].iChar + pSelectionMarker0->iChar;
        unsigned int iSelectionChar1 = pTL->pRuns[pSelectionMarker1->iRun].iChar + pSelectionMarker1->iChar;

        return pTL->pRuns[iRun].iChar < iSelectionChar1 && pTL->pRuns[iRun].iCharEnd > iSelectionChar0;
    }

    return false;
}
#endif

DRGUI_PRIVATE bool drgui_text_layout__get_selection_markers(drgui_text_layout* pTL, drgui_text_marker** ppSelectionMarker0Out, drgui_text_marker** ppSelectionMarker1Out)
{
    bool result = false;

    drgui_text_marker* pSelectionMarker0 = NULL;
    drgui_text_marker* pSelectionMarker1 = NULL;
    if (pTL != NULL && drgui_text_layout_is_anything_selected(pTL))
    {
        pSelectionMarker0 = &pTL->selectionAnchor;
        pSelectionMarker1 = &pTL->cursor;
        if (pTL->pRuns[pSelectionMarker0->iRun].iChar + pSelectionMarker0->iChar > pTL->pRuns[pSelectionMarker1->iRun].iChar + pSelectionMarker1->iChar)
        {
            drgui_text_marker* temp = pSelectionMarker0;
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


DRGUI_PRIVATE bool drgui_text_layout__first_line(drgui_text_layout* pTL, drgui_text_layout_line* pLine)
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

DRGUI_PRIVATE bool drgui_text_layout__next_line(drgui_text_layout* pTL, drgui_text_layout_line* pLine)
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

DRGUI_PRIVATE void drgui_text_layout__trim_undo_stack(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return;
    }

    while (pTL->undoStackCount > pTL->iUndoState)
    {
        unsigned int iLastItem = pTL->undoStackCount - 1;

        drgui_text_layout__uninit_undo_state(pTL->pUndoStack + iLastItem);
        pTL->undoStackCount -= 1;
    }
}

DRGUI_PRIVATE bool drgui_text_layout__diff_states(drgui_text_layout_state* pPrevState, drgui_text_layout_state* pCurrentState, drgui_text_layout_undo_state* pUndoStateOut)
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
    drgui__strncpy_s(pUndoStateOut->oldText, oldTextLen + 1, prevText + sameChCountStart, oldTextLen);

    size_t newTextLen = currLen - sameChCountStart - sameChCountEnd;
    pUndoStateOut->newText = malloc(newTextLen + 1);
    drgui__strncpy_s(pUndoStateOut->newText, newTextLen + 1, currText + sameChCountStart, newTextLen);

    return true;
}

DRGUI_PRIVATE void drgui_text_layout__uninit_undo_state(drgui_text_layout_undo_state* pUndoState)
{
    if (pUndoState == NULL) {
        return;
    }

    free(pUndoState->oldText);
    pUndoState->oldText = NULL;

    free(pUndoState->newText);
    pUndoState->newText = NULL;
}

DRGUI_PRIVATE void drgui_text_layout__push_undo_state(drgui_text_layout* pTL, drgui_text_layout_undo_state* pUndoState)
{
    if (pTL == NULL || pUndoState == NULL) {
        return;
    }

    assert(pTL->iUndoState == pTL->undoStackCount);


    drgui_text_layout_undo_state* pOldStack = pTL->pUndoStack;
    drgui_text_layout_undo_state* pNewStack = malloc(sizeof(*pNewStack) * (pTL->undoStackCount + 1));

    if (pTL->undoStackCount > 0) {
        memcpy(pNewStack, pOldStack, sizeof(*pNewStack) * (pTL->undoStackCount));
    }

    pNewStack[pTL->undoStackCount] = *pUndoState;
    pTL->pUndoStack = pNewStack;
    pTL->undoStackCount += 1;
    pTL->iUndoState += 1;

    if (pTL->onUndoPointChanged) {
        pTL->onUndoPointChanged(pTL, pTL->iUndoState);
    }

    free(pOldStack);
}

DRGUI_PRIVATE void drgui_text_layout__apply_undo_state(drgui_text_layout* pTL, drgui_text_layout_undo_state* pUndoState)
{
    if (pTL == NULL || pUndoState == NULL) {
        return;
    }

    // When undoing we want to remove the new text and replace it with the old text.

    unsigned int iFirstCh     = pUndoState->diffPos;
    unsigned int iLastChPlus1 = pUndoState->diffPos + strlen(pUndoState->newText);
    unsigned int bytesToRemove = iLastChPlus1 - iFirstCh;
    if (bytesToRemove > 0)
    {
        memmove(pTL->text + iFirstCh, pTL->text + iLastChPlus1, pTL->textLength - iLastChPlus1);
        pTL->textLength -= bytesToRemove;
        pTL->text[pTL->textLength] = '\0';
    }

    // TODO: This needs improving because it results in multiple onTextChanged and onDirty events being posted.

    // Insert the old text.
    drgui_text_layout_insert_text(pTL, pUndoState->oldText, pUndoState->diffPos);


    // The layout will have changed so it needs to be refreshed.
    drgui_text_layout__refresh(pTL);


    // Markers needs to be updated after refreshing the layout.
    drgui_text_layout__move_marker_to_character(pTL, &pTL->cursor, pUndoState->oldState.cursorPos);
    drgui_text_layout__move_marker_to_character(pTL, &pTL->selectionAnchor, pUndoState->oldState.selectionAnchorPos);

    // The cursor's sticky position needs to be updated whenever the text is edited.
    drgui_text_layout__update_marker_sticky_position(pTL, &pTL->cursor);

    // Ensure we mark the text as selected if appropriate.
    pTL->isAnythingSelected = pUndoState->oldState.isAnythingSelected;


    if (pTL->onTextChanged) {
        pTL->onTextChanged(pTL);
    }

    if (pTL->onDirty) {
        pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
    }
}

DRGUI_PRIVATE void drgui_text_layout__apply_redo_state(drgui_text_layout* pTL, drgui_text_layout_undo_state* pUndoState)
{
    if (pTL == NULL || pUndoState == NULL) {
        return;
    }

    // An redo is just the opposite of an undo. We want to remove the old text and replace it with the new text.

    unsigned int iFirstCh     = pUndoState->diffPos;
    unsigned int iLastChPlus1 = pUndoState->diffPos + strlen(pUndoState->oldText);
    unsigned int bytesToRemove = iLastChPlus1 - iFirstCh;
    if (bytesToRemove > 0)
    {
        memmove(pTL->text + iFirstCh, pTL->text + iLastChPlus1, pTL->textLength - iLastChPlus1);
        pTL->textLength -= bytesToRemove;
        pTL->text[pTL->textLength] = '\0';
    }

    // TODO: This needs improving because it results in multiple onTextChanged and onDirty events being posted.

    // Insert the new text.
    drgui_text_layout_insert_text(pTL, pUndoState->newText, pUndoState->diffPos);


    // The layout will have changed so it needs to be refreshed.
    drgui_text_layout__refresh(pTL);


    // Markers needs to be updated after refreshing the layout.
    drgui_text_layout__move_marker_to_character(pTL, &pTL->cursor, pUndoState->newState.cursorPos);
    drgui_text_layout__move_marker_to_character(pTL, &pTL->selectionAnchor, pUndoState->newState.selectionAnchorPos);

    // The cursor's sticky position needs to be updated whenever the text is edited.
    drgui_text_layout__update_marker_sticky_position(pTL, &pTL->cursor);

    // Ensure we mark the text as selected if appropriate.
    pTL->isAnythingSelected = pUndoState->newState.isAnythingSelected;


    if (pTL->onTextChanged) {
        pTL->onTextChanged(pTL);
    }

    if (pTL->onDirty) {
        pTL->onDirty(pTL, drgui_text_layout__local_rect(pTL));
    }
}


DRGUI_PRIVATE drgui_rect drgui_text_layout__local_rect(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return drgui_make_rect(0, 0, 0, 0);
    }

    return drgui_make_rect(0, 0, pTL->containerWidth, pTL->containerHeight);
}


DRGUI_PRIVATE void drgui_text_layout__on_cursor_move(drgui_text_layout* pTL)
{
    if (pTL == NULL) {
        return;
    }

    // When the cursor moves we want to reset the cursor's blink state.
    pTL->timeToNextCursorBlink = pTL->cursorBlinkRate;
    pTL->isCursorBlinkOn = true;

    if (pTL->onCursorMove) {
        pTL->onCursorMove(pTL);
    }
}
