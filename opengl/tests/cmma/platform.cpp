/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include <string>
 
//#include <android/log.h>

#include <GLES2/gl2.h>

#include "platform.h"
//#include "JavaClass.h"

using std::string;

namespace MaliSDK
{
    /**
     * Calls glGetError() until no more errors are reported.
     * GL stores all the errors since the last call to glGetError().
     * These are returned one-by-one with calls to getGetError(). Each call clears an error flag from GL.
     * If GL_NO_ERROR is returned no errors have occured since the last call to glGetError().
     * Prints the original error code the operation which caused the error (passed in) and the string version of the error.
     */
    void Platform::checkGlesError(const char* operation)
    {
        GLint error = 0;
        for (error = glGetError(); error != GL_NO_ERROR; error = glGetError())
        {
			fprintf(stderr,"glError (0x%x) after `%s` \n", error, operation);
            fprintf(stderr,"glError (0x%x) = `%s` \n", error, glErrorToString(error));
        }
    }

    /**
     * Converts the error codes into strings using the definitions found in <GLES2/gl2.h>
     */
    const char* Platform::glErrorToString(int glErrorCode)
    {
        switch(glErrorCode)
        {
            case GL_NO_ERROR: 
                return "GL_NO_ERROR"; 
                break;
            case GL_INVALID_ENUM:
                return "GL_INVALID_ENUM"; 
                break;
            case GL_INVALID_VALUE: 
                return "GL_INVALID_VALUE";
                break;
            case GL_INVALID_OPERATION: 
                return "GL_INVALID_OPERATION"; 
                break;
            case GL_OUT_OF_MEMORY: 
                return "GL_OUT_OF_MEMORY"; 
                break;
            case GL_INVALID_FRAMEBUFFER_OPERATION:
                return "GL_INVALID_FRAMEBUFFER_OPERATION";
                break;
            default:   
                return "unknown"; 
                break;
        }
    }
#if 0
    char* AndroidPlatform::copyString(const char* string)
    {
        int length = 0;
        char *newString = NULL;
        if (string == NULL)
        {
            return NULL;
        }
        length = strlen(string) + 1;
        newString = (char*)malloc(length);
        if (newString == NULL)
        {
            LOGE("copyString(): Failed to allocate memory using malloc().\n");
            return NULL;
        }
        memcpy(newString, string, length);
        return newString;
    }
#endif

}
