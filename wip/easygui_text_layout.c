// Public domain. See "unlicense" statement at the end of this file.

#include "easygui_text_layout.h"

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


    /// The size of the extra data.
    size_t extraDataSize;

    /// A pointer to the extra data.
    char pExtraData[1];
};


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


void easygui_iterate_visible_text_runs(easygui_text_layout* pTL, easygui_text_layout_run_iterator_proc callback, void* pUserData)
{
    if (pTL == NULL || callback == NULL) {
        return;
    }

    // NOTE: THIS IS A TEMPORARY IMPLEMENTATION JUST TO GET SOMETHING BASIC WORKING.


    easygui_text_run run;
    run.posX            = 0;
    run.posY            = 0;
    run.width           = 0;
    run.height          = 0;
    run.pFont           = pTL->pDefaultFont;
    run.textColor       = pTL->defaultTextColor;
    run.backgroundColor = pTL->defaultBackgroundColor;

    easygui_font_metrics fontMetrics;
    easygui_get_font_metrics(run.pFont, 1, 1, &fontMetrics);
    

    // Iterate over each line and just increment the y pen position.
    const char* text = pTL->text;
    if (text == NULL) {
        return;
    }

    const char* lineStart = pTL->text;
    const char* lineEnd   = lineStart;

    while (text[0] != '\0')
    {
        if (text[0] == '\n')
        {
            // We're at the end of the line.
            lineEnd = text;


            run.text = lineStart;
            run.textLength = (unsigned int)(lineEnd - lineStart);
            callback(pTL, &run, pUserData);

            run.posY += fontMetrics.lineHeight;
            lineStart = lineEnd + 1;
        }

        text += 1;
    }

    lineEnd = text;
    if (lineStart < lineEnd)
    {
        run.text = lineStart;
        run.textLength = (unsigned int)(lineEnd - lineStart);
        callback(pTL, &run, pUserData);
    }
}
