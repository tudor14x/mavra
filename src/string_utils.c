#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "string_utils.h"

bool strBeginsWith(const char *str, const char *control, bool caseSensitive)
{
    if(caseSensitive) {
        return strncmp(str, control, strlen(control)) == 0;
    }

    int ctrlen = strlen(control), strrlen = strlen(str);
    if (ctrlen > strrlen)
    {
        return false;
    }

    for (int i = 0; i < ctrlen; ++i)
    {
        if (isalpha(str[i]))
        {
            if (tolower(str[i]) != tolower(control[i]))
            {
                return false;
            }
        }
        else
        {
            if (control[i] != str[i])
            {
                return false;
            }
        }
    }

    return true;
}

void strToLower(char *str)
{
    for (int i = 0; i < strlen(str); ++i)
    {
        str[i] = tolower(str[i]);
    }
}

int strOccurence(const char *str, const char *substr)
{
    char *ptr = strstr(str, substr);
    if (!ptr)
    {
        return strlen(str);
    }
    return (int)(ptr - str);
}

void strStripWhitespace(char **str)
{
    for(int i = 0; i < strlen(*str); ++i) {
        if(isspace((*str)[i])) {
            (*str)++;
            i--;
        } else {
            break;
        }
    }
    
    for(int i = strlen(*str)-1; i >= 0; --i) {
        if(isspace((*str)[i])) {
            (*str)[i] = 0;
        } else {
            break;
        }
    }
}

bool strIsInteger(const char *str)
{
    char *temp = strdup(str);

    strStripWhitespace(&temp);

    for(int i = 0; i < strlen(temp); ++i) {
        if(!(isalnum(temp[i]) && !isalpha(temp[i]))) {
            free(temp);
            return false;
        }
    }

    free(temp);

    return true;
}