//
//  main.c
//  iris
//
//  Created by Corey Clayton on 2012-11-04.
//  Copyright (c) 2012 Awake Coding. All rights reserved.
//


#include <ApplicationServices/ApplicationServices.h>
#include <stdio.h>
#include "OpenGL/OpenGL.h"
#include "OpenGL/gl.h"
#include "CoreFoundation/CoreFoundation.h"

#include "CoreVideo/CoreVideo.h"



CGLContextObj glContext;
CGContextRef bmp;
CGImageRef img;

/*
 * perform an in-place swap from Quadrant 1 to Quadrant III format
 * (upside-down PostScript/GL to right side up QD/CG raster format)
 * We do this in-place, which requires more copying, but will touch
 * only half the pages.  (Display grabs are BIG!)
 *
 * Pixel reformatting may optionally be done here if needed.
 */
static void swizzleBitmap(void * data, int rowBytes, int height)
{
    int top, bottom;
    void * buffer;
    void * topP;
    void * bottomP;
    void * base;
    
    
    top = 0;
    bottom = height - 1;
    base = data;
    buffer = malloc(rowBytes);
    
    
    while ( top < bottom )
    {
        topP = (void *)((top * rowBytes) + (intptr_t)base);
        bottomP = (void *)((bottom * rowBytes) + (intptr_t)base);
        
        
        /*
         * Save and swap scanlines.
         *
         * This code does a simple in-place exchange with a temp buffer.
         * If you need to reformat the pixels, replace the first two bcopy()
         * calls with your own custom pixel reformatter.
         */
        bcopy( topP, buffer, rowBytes );
        bcopy( bottomP, topP, rowBytes );
        bcopy( buffer, bottomP, rowBytes );
        
        ++top;
        --bottom;
    }
    free( buffer );
}

int oldCapture()
{
    printf("Get the display id...\n");

    CGDirectDisplayID display_id;
    display_id = CGMainDisplayID();
    
    CGLPixelFormatAttribute pfa[] = {
        kCGLPFAFullScreen,
        kCGLPFADisplayMask,
        0,    // display mask
        0
    } ;
    
    pfa[2] = CGDisplayIDToOpenGLDisplayMask(display_id);
    
    printf("\tdisplay = %#0X\n", display_id);
    
    ////////////////
    
    printf("Create OpenGL context...\n");
    
    CGLPixelFormatObj pFormat;
    int numFormats;
    
    CGLChoosePixelFormat(pfa, &pFormat, &numFormats);
    
    if (pFormat == NULL)
    {
        printf("Failed to set pixel format\n");
        return 1;
    }
    
    CGLCreateContext(pFormat, 0, &glContext);
    CGLDestroyPixelFormat(pFormat);
    
    if (glContext == NULL)
    {
        printf("Failed to create OpenGL context\n");
        return 1;
    }
    
    ///////////////
    
    printf("Create bitmap context...\n");
    
    GLint width = 2880;
    GLint height = 1800;
    CGRect rect;
    rect.origin.x = 0;
    rect.origin.y = 0;
    rect.size.height = height;
    rect.size.width = width;
    
    
    CGLSetCurrentContext(glContext);
    CGLSetFullScreenOnDisplay(glContext, display_id);
    
    glReadBuffer(GL_FRONT);
    
    long stride;
    long bytes;
    
    stride = width * 4;
    stride = (stride + 3) & ~3;
    bytes = stride * height;
    
    void * data;
    
    data = malloc(bytes);
    if (data == NULL)
    {
        printf("malloc failed!!!\n");
        CGLSetCurrentContext(NULL);
        CGLClearDrawable(glContext);
        CGLDestroyContext(glContext);
        
        return 1;
    }
    
    
    CGColorSpaceRef cSpace = CGColorSpaceCreateWithName (kCGColorSpaceGenericRGB);
    bmp = CGBitmapContextCreate(data, width, height, 8, stride, cSpace, kCGImageAlphaNoneSkipFirst);
    CFRelease(cSpace);
    
    if (bmp == NULL)
    {
        printf("failed to create bitmap context\n");
        return 1;
    }
    
    printf("Trying to grab image...\n");
    
    // Read from framebuffer
    glFinish();
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    
    glReadPixels((GLint)rect.origin.x, (GLint)rect.origin.y, width, height,
                 GL_BGRA,
#ifdef __BIG_ENDIAN__
                 GL_UNSIGNED_INT_8_8_8_8_REV, // for PPC
#else
                 GL_UNSIGNED_INT_8_8_8_8, // for Intel! http://lists.apple.com/archives/quartz-dev/2006/May/msg00100.html
#endif
                 data);
    
    //may want to work our magic here
    
    
    
    img = CGBitmapContextCreateImage(bmp);

    
    //now let us try to write to a file
    CFURLRef urlpath;
    
    char path[] = "/Users/corey/test.png";
    
    urlpath = CFURLCreateFromFileSystemRepresentation(NULL, (unsigned char *) path, strlen(path), FALSE);
    
    CGImageDestinationRef destination = CGImageDestinationCreateWithURL(urlpath, kUTTypePNG, 1, NULL);
    CGImageDestinationAddImage(destination, img, nil);
    
    if (!CGImageDestinationFinalize(destination))
    {
        printf("Failed to write image to %s\n", path);
    }
    
    CFRelease(destination);
    
    
    
    CFRelease(bmp);
    free(data);
    
    ///////////
    CGLSetCurrentContext(NULL);
    CGLClearDrawable(glContext);
    CGLDestroyContext(glContext);
    printf("Done!\n");
    return 0;
}

int method2()
{
    int bytewidth;
    
    CGImageRef image = CGDisplayCreateImage(kCGDirectMainDisplay);    // Main screenshot capture call
    
    CGSize frameSize = CGSizeMake(CGImageGetWidth(image), CGImageGetHeight(image));    // Get screenshot bounds
    
    /*
    NSDictionary *options = [NSDictionary dictionaryWithObjectsAndKeys:
                             [NSNumber numberWithBool:NO], kCVPixelBufferCGImageCompatibilityKey,
                             [NSNumber numberWithBool:NO], kCVPixelBufferCGBitmapContextCompatibilityKey,
                             nil];*/
    
    CFDictionaryRef opts;
    
    long ImageCompatibility;
    long BitmapContextCompatibility;
    
    void * keys[3];
    keys[0] = (void *) kCVPixelBufferCGImageCompatibilityKey;
    keys[1] = (void *) kCVPixelBufferCGBitmapContextCompatibilityKey;
    keys[2] = NULL;
    
    void * values[3];
    values[0] = (void *) &ImageCompatibility;
    values[1] = (void *) &BitmapContextCompatibility;
    values[2] = NULL;
    
    opts = CFDictionaryCreate(kCFAllocatorDefault, (const void **) keys, (const void **) values, 2, NULL, NULL);
    
    if (opts == NULL)
    {
        printf("failed to create dictionary\n");
        return 1;
    }
    
    CVPixelBufferRef pxbuffer = NULL;
    CVReturn status = CVPixelBufferCreate(kCFAllocatorDefault, frameSize.width,
                                          frameSize.height,  kCVPixelFormatType_32ARGB, opts,
                                          &pxbuffer);
    
    if (status != kCVReturnSuccess)
    {
        printf("Failed to create pixel buffer! \n");
        return 1;
    }
    
    CVPixelBufferLockBaseAddress(pxbuffer, 0);
    void *pxdata = CVPixelBufferGetBaseAddress(pxbuffer);
    
    CGColorSpaceRef rgbColorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(pxdata, frameSize.width,
                                                 frameSize.height, 8, 4*frameSize.width, rgbColorSpace,
                                                 kCGImageAlphaNoneSkipLast);
    
    CGContextDrawImage(context, CGRectMake(0, 0, CGImageGetWidth(image),
                                           CGImageGetHeight(image)), image);
    
    bytewidth = frameSize.width * 4; // Assume 4 bytes/pixel for now
    bytewidth = (bytewidth + 3) & ~3; // Align to 4 bytes
    swizzleBitmap(pxdata, bytewidth, frameSize.height);     // Solution for ARGB madness
    
    
    //now let us try to write to a file
    CFURLRef urlpath;
    
    char path[] = "/Users/corey/test2.png";
    
    urlpath = CFURLCreateFromFileSystemRepresentation(NULL, (const unsigned char *) path, strlen(path), FALSE);
    
    CGImageDestinationRef destination = CGImageDestinationCreateWithURL(urlpath, kUTTypePNG, 1, NULL);
    CGImageDestinationAddImage(destination, image, nil);
    
    if (!CGImageDestinationFinalize(destination))
    {
        printf("Failed to write image to %s\n", path);
    }
    
    CFRelease(destination);
    //////////////
    
    
    CGColorSpaceRelease(rgbColorSpace);
    CGImageRelease(image);
    CGContextRelease(context);
    
    CVPixelBufferUnlockBaseAddress(pxbuffer, 0);
    
    //return pxbuffer;
    return 0;
}


int main(int argc, const char * argv[])
{
    method2();
    return 0;
}

