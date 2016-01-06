// Public domain. See "unlicense" statement at the end of this file.

#include "easygui_text_layout.h"
#include <assert.h>

#ifndef OUT
#define OUT
#endif

#ifndef DO_NOTHING
#define DO_NOTHING
#endif

struct easygui_text_layout
{
    /// The main text of the layout.
    char* text;

    /// The length of the text.
    size_t textLength;


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

    /// The size of a tab in spaces.
    unsigned int tabSizeInSpaces;

    /// The horizontal alignment.
    easygui_text_layout_alignment horzAlign;

    /// The vertical alignment.
    easygui_text_layout_alignment vertAlign;


    /// The total width of the text.
    float textBoundsWidth;

    /// The total height of the text.
    float textBoundsHeight;


    /// A pointer to the buffer containing details about every run in the layout.
    easygui_text_run* pRuns;

    /// The number of runs in <pRuns>
    size_t runCount;

    /// The size of the <pRuns> buffer in easygui_text_run's. This is used to determine whether or not the buffer
    /// needs to be reallocated upon adding a new run.
    size_t runBufferSize;


    /// The size of the extra data.
    size_t extraDataSize;

    /// A pointer to the extra data.
    char pExtraData[1];
};

typedef struct
{
    /// The index of the run within the line the marker is positioned on.
    unsigned int iRun;

    /// The index of the character within the run the marker is positioned to the left of.
    unsigned int iChar;

    /// The position on the x axis, relative to the x position of the run.
    int relativePosX;

    /// The absolute position on the x axis to place the marker when moving up and down lines. Note that this is not relative
    /// to the run, but rather the line. This will be updated when the marker is moved left and right.
    int absoluteSickyPosX;

} easygui_text_marker;


/// Performs a complete refresh of the given text layout.
///
/// @remarks
///     This will delete every run and re-create them.
PRIVATE void easygui_refresh_text_layout(easygui_text_layout* pTL);

/// Refreshes the alignment of the given text layout.
PRIVATE void easygui_refresh_text_layout_alignment(easygui_text_layout* pTL);

/// Appends a text run to the list of runs in the given text layout.
PRIVATE void easygui_push_text_run(easygui_text_layout* pTL, easygui_text_run* pRun);

/// Clears the internal list of text runs.
PRIVATE void easygui_clear_text_runs(easygui_text_layout* pTL);

/// Helper for calculating the offset to apply to each line based on the alignment of the given text layout.
PRIVATE void easygui_calculate_line_alignment_offset(easygui_text_layout* pTL, float lineWidth, float* pOffsetXOut, float* pOffsetYOut);

/// Helper for determine whether or not the given text run is whitespace.
PRIVATE bool easygui_is_text_run_whitespace(easygui_text_layout* pTL, easygui_text_run* pRun);


easygui_text_layout* easygui_create_text_layout(easygui_context* pContext, size_t extraDataSize, void* pExtraData)
{
    if (pContext == NULL) {
        return NULL;
    }

    easygui_text_layout* pTL = malloc(sizeof(easygui_text_layout) - sizeof(pTL->pExtraData) + extraDataSize);
    if (pTL == NULL) {
        return NULL;
    }

    pTL->text                   = NULL;
    pTL->textLength             = 0;
    pTL->containerWidth         = 0;
    pTL->containerHeight        = 0;
    pTL->innerOffsetX           = 0;
    pTL->innerOffsetY           = 0;
    pTL->pDefaultFont           = NULL;
    pTL->defaultTextColor       = easygui_rgb(224, 224, 224);
    pTL->defaultBackgroundColor = easygui_rgb(48, 48, 48);
    pTL->tabSizeInSpaces        = 4;
    pTL->horzAlign              = easygui_text_layout_alignment_left;
    pTL->vertAlign              = easygui_text_layout_alignment_top;
    pTL->textBoundsWidth        = 0;
    pTL->textBoundsHeight       = 0;
    pTL->pRuns                  = NULL;
    pTL->runCount               = 0;
    pTL->runBufferSize          = 0;

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

    free(pTL);
}


size_t easygui_get_text_layout_extra_data_size(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return 0;
    }

    return pTL->extraDataSize;
}

void* easygui_get_text_layout_extra_data(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return NULL;
    }

    return pTL->pExtraData;
}


void easygui_set_text_layout_text(easygui_text_layout* pTL, const char* text)
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
    easygui_refresh_text_layout(pTL);
}

size_t easygui_get_text_layout_text(easygui_text_layout* pTL, char* textOut, size_t textOutSize)
{
    if (pTL == NULL) {
        return 0;
    }

    if (textOut == NULL) {
        return pTL->textLength;
    }


    if (strcpy_s(textOut, textOutSize, pTL->text) == 0) {
        return pTL->textLength;
    }

    return 0;   // Error with strcpy_s().
}


void easygui_set_text_layout_container_size(easygui_text_layout* pTL, float containerWidth, float containerHeight)
{
    if (pTL == NULL) {
        return;
    }

    pTL->containerWidth  = containerWidth;
    pTL->containerHeight = containerHeight;
}

void easygui_get_text_layout_container_size(easygui_text_layout* pTL, float* pContainerWidthOut, float* pContainerHeightOut)
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


void easygui_set_text_layout_inner_offset(easygui_text_layout* pTL, float innerOffsetX, float innerOffsetY)
{
    if (pTL == NULL) {
        return;
    }

    pTL->innerOffsetX = innerOffsetX;
    pTL->innerOffsetY = innerOffsetY;
}

void easygui_get_text_layout_inner_offset(easygui_text_layout* pTL, float* pInnerOffsetX, float* pInnerOffsetY)
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


void easygui_set_text_layout_default_font(easygui_text_layout* pTL, easygui_font* pFont)
{
    if (pTL == NULL) {
        return;
    }

    pTL->pDefaultFont = pFont;

    // A change in font requires a layout refresh.
    easygui_refresh_text_layout(pTL);
}

easygui_font* easygui_get_text_layout_default_font(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return NULL;
    }

    return pTL->pDefaultFont;
}

void easygui_set_text_layout_default_text_color(easygui_text_layout* pTL, easygui_color color)
{
    if (pTL == NULL) {
        return;
    }

    pTL->defaultTextColor = color;
}

easygui_color easygui_get_text_layout_default_text_color(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return easygui_rgb(0, 0, 0);
    }

    return pTL->defaultTextColor;
}

void easygui_set_text_layout_default_bg_color(easygui_text_layout* pTL, easygui_color color)
{
    if (pTL == NULL) {
        return;
    }

    pTL->defaultBackgroundColor = color;
}

easygui_color easygui_get_text_layout_default_bg_color(easygui_text_layout* pTL)
{
    return pTL->defaultBackgroundColor;
}


void easygui_set_text_layout_tab_size(easygui_text_layout* pTL, unsigned int sizeInSpaces)
{
    if (pTL == NULL) {
        return;
    }

    if (pTL->tabSizeInSpaces != sizeInSpaces)
    {
        pTL->tabSizeInSpaces = sizeInSpaces;
        easygui_refresh_text_layout(pTL);
    }
}

unsigned int easygui_get_text_layout_tab_size(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return 0;
    }

    return pTL->tabSizeInSpaces;
}


void easygui_set_text_layout_horizontal_align(easygui_text_layout* pTL, easygui_text_layout_alignment alignment)
{
    if (pTL == NULL) {
        return;
    }

    if (pTL->horzAlign != alignment)
    {
        pTL->horzAlign = alignment;
        easygui_refresh_text_layout_alignment(pTL);
    }
}

easygui_text_layout_alignment easygui_get_text_layout_horizontal_align(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return easygui_text_layout_alignment_left;
    }

    return pTL->horzAlign;
}

void easygui_set_text_layout_vertical_align(easygui_text_layout* pTL, easygui_text_layout_alignment alignment)
{
    if (pTL == NULL) {
        return;
    }

    if (pTL->vertAlign != alignment)
    {
        pTL->vertAlign = alignment;
        easygui_refresh_text_layout_alignment(pTL);
    }
}

easygui_text_layout_alignment easygui_get_text_layout_vertical_align(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return easygui_text_layout_alignment_top;
    }

    return pTL->vertAlign;
}


easygui_rect easygui_get_text_layout_text_rect_relative_to_bounds(easygui_text_layout* pTL)
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


void easygui_iterate_visible_text_runs(easygui_text_layout* pTL, easygui_text_layout_run_iterator_proc callback, void* pUserData)
{
    if (pTL == NULL || callback == NULL) {
        return;
    }

    // The position of each run will be relative to the text bounds. We want to make it relative to the container bounds.
    easygui_rect textRect = easygui_get_text_layout_text_rect_relative_to_bounds(pTL);

    // This is a naive implementation. Can be improved a bit.
    for (size_t iRun = 0; iRun < pTL->runCount; ++iRun)
    {
        easygui_text_run* pRun = pTL->pRuns + iRun;

        if (!easygui_is_text_run_whitespace(pTL, pRun))
        {
            float runTop    = pRun->posY + textRect.top;
            float runBottom = runTop     + pRun->height;

            if (runBottom > 0 && runTop < pTL->containerHeight)
            {
                float runLeft  = pRun->posX + textRect.left;
                float runRight = runLeft    + pRun->width;

                if (runRight > 0 && runLeft < pTL->containerWidth)
                {
                    // The run is visible.
                    easygui_text_run run = pTL->pRuns[iRun];
                    run.pFont           = pTL->pDefaultFont;
                    run.textColor       = pTL->defaultTextColor;
                    run.backgroundColor = pTL->defaultBackgroundColor;
                    run.text            = pTL->text + run.iChar;
                    run.posX            = runLeft;
                    run.posY            = runTop;
                    callback(pTL, &run, pUserData);
                }
            }
        }
    }

#if 0
    for (size_t iRun = 0; iRun < pTL->runCount; ++iRun)
    {
        easygui_text_run run = pTL->pRuns[iRun];
        run.pFont           = pTL->pDefaultFont;
        run.textColor       = pTL->defaultTextColor;
        run.backgroundColor = pTL->defaultBackgroundColor;
        run.text            = pTL->text + run.iChar;
        callback(pTL, &run, pUserData);
    }
#endif
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

PRIVATE void easygui_refresh_text_layout(easygui_text_layout* pTL)
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
    easygui_clear_text_runs(pTL);

    // The text bounds also need to be reset at the top.
    pTL->textBoundsWidth  = 0;
    pTL->textBoundsHeight = 0;


    easygui_font_metrics defaultFontMetrics;
    easygui_get_font_metrics(pTL->pDefaultFont, 1, 1, &defaultFontMetrics);

    const unsigned int tabWidth = defaultFontMetrics.spaceWidth * pTL->tabSizeInSpaces;

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
            run.width  = (float)(((tabCount*tabWidth) - ((unsigned int)run.posX % tabWidth)));
            run.height = (float)defaultFontMetrics.lineHeight;
        }
        else if (nextRunStart[0] == '\n')
        {
            // New line.
            iCurrentLine += 1;
            run.width  = 0;
            run.height = (float)defaultFontMetrics.lineHeight;

            // A new line means we need to increment the running y position by the running line height.
            runningPosY += runningLineHeight;
            runningLineHeight = 0;
        }
        else if (nextRunStart[0] == '\0')
        {
            // Null terminator.
            run.width  = 0;
            run.height = 0;
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


        // Add the run to the internal list.
        easygui_push_text_run(pTL, &run);

        // Go to the next run string.
        nextRunStart = nextRunEnd;
    }

    // If we were to return now the text would be alignment top/left. If the alignment is not top/left we need to refresh the layout.
    if (pTL->horzAlign != easygui_text_layout_alignment_left || pTL->vertAlign != easygui_text_layout_alignment_top)
    {
        easygui_refresh_text_layout_alignment(pTL);
    }
}

PRIVATE void easygui_refresh_text_layout_alignment(easygui_text_layout* pTL)
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
        easygui_calculate_line_alignment_offset(pTL, lineWidth, OUT &lineOffsetX, OUT &lineOffsetY);

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

void easygui_calculate_line_alignment_offset(easygui_text_layout* pTL, float lineWidth, float* pOffsetXOut, float* pOffsetYOut)
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


PRIVATE void easygui_push_text_run(easygui_text_layout* pTL, easygui_text_run* pRun)
{
    if (pTL == NULL) {
        return;
    }

    if (pTL->runBufferSize == pTL->runCount)
    {
        size_t newRunBufferSize = pTL->runBufferSize*2;
        if (newRunBufferSize == 0) {
            newRunBufferSize = 1;
        }

        easygui_text_run* pOldRuns = pTL->pRuns;
        easygui_text_run* pNewRuns = malloc(sizeof(easygui_text_run) * newRunBufferSize);     // +1 just to make sure we have at least 1 item in the buffer.

        memcpy(pNewRuns, pOldRuns, sizeof(easygui_text_run) * pTL->runCount);

        pTL->pRuns = pNewRuns;
        pTL->runBufferSize = newRunBufferSize;

        free(pOldRuns);
    }

    pTL->pRuns[pTL->runCount] = *pRun;
    pTL->runCount += 1;
}

PRIVATE void easygui_clear_text_runs(easygui_text_layout* pTL)
{
    if (pTL == NULL) {
        return;
    }

    pTL->runCount = 0;
}

PRIVATE bool easygui_is_text_run_whitespace(easygui_text_layout* pTL, easygui_text_run* pRun)
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

