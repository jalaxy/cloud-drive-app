#include <readconfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <wchar.h>
#include <string.h>
#define IS_SEP(wc) ((wc) == L' ' || (wc) == L'\n' || (wc) == L'\r' || (wc) == L'\t')

typedef struct list
{
    wchar_t *name, *value;
    struct list *next;
} list;

/**
 * @brief Allocate memory, read file and convert to unicode
 *
 * @param fname file name
 * @param locale system locale
 * @param pwsz pointer to file size
 */
wchar_t *alloc_unicode_from_file(const char *fname, const char *locale, int *pwsz)
{
    FILE *fp = fopen(fname, "rb");
    if (fp == NULL)
        return NULL;
    char ch;
    fseek(fp, 0, SEEK_END);
    int sz = ftell(fp);
    rewind(fp);
    char *mbs = (char *)malloc(sizeof(char) * (sz + 1));
    fread(mbs, sizeof(char), sz, fp);
    mbs[sz] = '\0';
    setlocale(LC_ALL, locale);
    *pwsz = mbstowcs(NULL, mbs, 0);
    wchar_t *wcs = (wchar_t *)malloc(sizeof(wchar_t) * (*pwsz));
    mbstowcs(wcs, mbs, *pwsz);
    free(mbs);
    fclose(fp);
    return wcs;
}

/**
 * @brief Parse configuration file and allocate memory simultaneously
 *
 * @param names pointer to array of the variable name
 * @param values pointer to array of the value of each name
 * @return number of variables
 */
int alloc_and_parse_config(
    const char *fname, const char *locale, char **pnames[], char **pvalues[])
{
    int wlen;
    wchar_t *wstr = alloc_unicode_from_file(fname, locale, &wlen);
    if (wstr == NULL)
        return 0;
    int i = 0;
    list head;
    head.name = NULL;
    head.next = NULL;
    while (i < wlen)
        if (wstr[i] == L'=')
        {
            int i_j = i - 1, i_k = i + 1; // (j, i_j] = [i_k, k)
            while (IS_SEP(wstr[i_j]))
                i_j--;
            while (IS_SEP(wstr[i_k]))
                i_k++;
            int j = i_j, k = i_k;
            while (j >= 0 && !IS_SEP(wstr[j]))
                j--;
            while (k < wlen && !IS_SEP(wstr[k]))
                k++;
            wchar_t *wname = (wchar_t *)malloc(sizeof(wchar_t) * (i_j - j + 1));
            if (wname == NULL)
                return 0;
            memcpy(wname, wstr + j + 1, (i_j - j) * sizeof(wchar_t));
            wname[i_j - j] = L'\0';
            wchar_t *wvalue = (wchar_t *)malloc(sizeof(wchar_t *) * (k - i_k + 1));
            if (wvalue == NULL)
                return 0;
            memcpy(wvalue, wstr + i_k, (k - i_k) * sizeof(wchar_t));
            wvalue[k - i_k] = L'\0';
            list *p = &head;
            while (p->next != NULL && wcscmp(p->next->name, wname) != 0)
                p = p->next;
            if (p->next == NULL)
            {
                p = p->next = (list *)malloc(sizeof(list));
                p->next = NULL;
                p->name = wname;
                p->value = wvalue;
            }
            else
            {
                free(wname);
                free(p->value);
                p->value = wvalue;
            }
            i = k + 1;
        }
        else
            i++;
    int num = 0;
    for (list *p = head.next; p; p = p->next)
        num++;
    *pnames = (char **)malloc(sizeof(char *) * num);
    *pvalues = (char **)malloc(sizeof(char *) * num);
    memset((*pnames), 0, sizeof(char *) * num);
    memset((*pvalues), 0, sizeof(char *) * num);
    num = 0;
    for (list *p = head.next; p;)
    {
        int lname = wcstombs(NULL, p->name, 0);
        (*pnames)[num] = (char *)malloc(sizeof(char) * (lname + 1));
        if ((*pnames)[num] == NULL)
            return 0;
        wcstombs((*pnames)[num], p->name, lname);
        (*pnames)[num][lname] = '\0';
        free(p->name);
        int lvalue = wcstombs(NULL, p->value, 0);
        (*pvalues)[num] = (char *)malloc(sizeof(char) * (lvalue + 1));
        if ((*pvalues)[num] == NULL)
            return 0;
        wcstombs((*pvalues)[num], p->value, lname);
        (*pvalues)[num][lvalue] = '\0';
        free(p->value);
        list *q = p;
        p = p->next;
        num++;
        free(q);
    }
    free(wstr);
    return num;
}

/**
 * @brief Find the index of a certain names
 *
 * @param str the string to find
 * @param n size of names array
 * @param names the names array
 */
int query_config(char *str, int n, char *names[])
{
    while (n--)
        if (strcmp(str, names[n]) == 0)
            return n;
    return 0;
}

/**
 * @brief Release the space of allocation previously
 *
 * @param n size of configuration array
 * @param names variable names
 * @param values variable values
 */
void free_config(int n, char *names[], char *values[])
{
    while (n--)
    {
        free(names[n]);
        free(values[n]);
    }
}
