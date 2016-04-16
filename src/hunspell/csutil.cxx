/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Hunspell, based on MySpell.
 *
 * The Initial Developers of the Original Code are
 * Kevin Hendricks (MySpell) and Németh László (Hunspell).
 * Portions created by the Initial Developers are Copyright (C) 2002-2005
 * the Initial Developers. All Rights Reserved.
 *
 * Contributor(s): David Einstein, Davide Prina, Giuseppe Modugno,
 * Gianluca Turconi, Simon Brouwer, Noll János, Bíró Árpád,
 * Goldman Eleonóra, Sarlós Tamás, Bencsáth Boldizsár, Halácsy Péter,
 * Dvornik László, Gefferth András, Nagy Viktor, Varga Dániel, Chris Halls,
 * Rene Engelhard, Bram Moolenaar, Dafydd Jones, Harri Pitkänen
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
/*
 * Copyright 2002 Kevin B. Hendricks, Stratford, Ontario, Canada
 * And Contributors.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. All modifications to the source code must be clearly marked as
 *    such.  Binary redistributions based on modified source code
 *    must be clearly marked as modified versions in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY KEVIN B. HENDRICKS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * KEVIN B. HENDRICKS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <algorithm>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "csutil.hxx"
#include "atypes.hxx"
#include "langnum.hxx"

// Unicode character encoding information
struct unicode_info {
  unsigned short c;
  unsigned short cupper;
  unsigned short clower;
};

#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
#endif

#include "utf_info.cxx"
#define UTF_LST_LEN (sizeof(utf_lst) / (sizeof(unicode_info)))

struct unicode_info2 {
  char cletter;
  unsigned short cupper;
  unsigned short clower;
};

static struct unicode_info2* utf_tbl = NULL;
static int utf_tbl_count =
    0;  // utf_tbl can be used by multiple Hunspell instances

FILE* myfopen(const char* path, const char* mode) {
#ifdef _WIN32
#define WIN32_LONG_PATH_PREFIX "\\\\?\\"
  if (strncmp(path, WIN32_LONG_PATH_PREFIX, 4) == 0) {
    int len = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    wchar_t* buff = (wchar_t*)malloc(len * sizeof(wchar_t));
    wchar_t* buff2 = (wchar_t*)malloc(len * sizeof(wchar_t));
    FILE* f = NULL;
    if (buff && buff2) {
      MultiByteToWideChar(CP_UTF8, 0, path, -1, buff, len);
      if (_wfullpath(buff2, buff, len) != NULL) {
        f = _wfopen(buff2, (strcmp(mode, "r") == 0) ? L"r" : L"rb");
      }
      free(buff);
      free(buff2);
    }
    return f;
  }
#endif
  return fopen(path, mode);
}

std::string& u16_u8(std::string& dest, const std::vector<w_char>& src) {
  dest.clear();
  std::vector<w_char>::const_iterator u2 = src.begin();
  std::vector<w_char>::const_iterator u2_max = src.end();
  while (u2 < u2_max) {
    signed char u8;
    if (u2->h) {  // > 0xFF
      // XXX 4-byte haven't implemented yet.
      if (u2->h >= 0x08) {  // >= 0x800 (3-byte UTF-8 character)
        u8 = 0xe0 + (u2->h >> 4);
        dest.push_back(u8);
        u8 = 0x80 + ((u2->h & 0xf) << 2) + (u2->l >> 6);
        dest.push_back(u8);
        u8 = 0x80 + (u2->l & 0x3f);
        dest.push_back(u8);
      } else {  // < 0x800 (2-byte UTF-8 character)
        u8 = 0xc0 + (u2->h << 2) + (u2->l >> 6);
        dest.push_back(u8);
        u8 = 0x80 + (u2->l & 0x3f);
        dest.push_back(u8);
      }
    } else {               // <= 0xFF
      if (u2->l & 0x80) {  // >0x80 (2-byte UTF-8 character)
        u8 = 0xc0 + (u2->l >> 6);
        dest.push_back(u8);
        u8 = 0x80 + (u2->l & 0x3f);
        dest.push_back(u8);
      } else {  // < 0x80 (1-byte UTF-8 character)
        u8 = u2->l;
        dest.push_back(u8);
      }
    }
    ++u2;
  }
  return dest;
}

int u8_u16(std::vector<w_char>& dest, const std::string& src) {
  dest.clear();
  std::string::const_iterator u8 = src.begin();
  std::string::const_iterator u8_max = src.end();

  while (u8 < u8_max) {
    w_char u2;
    switch ((*u8) & 0xf0) {
      case 0x00:
      case 0x10:
      case 0x20:
      case 0x30:
      case 0x40:
      case 0x50:
      case 0x60:
      case 0x70: {
        u2.h = 0;
        u2.l = *u8;
        break;
      }
      case 0x80:
      case 0x90:
      case 0xa0:
      case 0xb0: {
        HUNSPELL_WARNING(stderr,
                         "UTF-8 encoding error. Unexpected continuation bytes "
                         "in %ld. character position\n%s\n",
                         static_cast<long>(std::distance(src.begin(), u8)),
                         src.c_str());
        u2.h = 0xff;
        u2.l = 0xfd;
        break;
      }
      case 0xc0:
      case 0xd0: {  // 2-byte UTF-8 codes
        if ((*(u8 + 1) & 0xc0) == 0x80) {
          u2.h = (*u8 & 0x1f) >> 2;
          u2.l = (*u8 << 6) + (*(u8 + 1) & 0x3f);
          ++u8;
        } else {
          HUNSPELL_WARNING(stderr,
                           "UTF-8 encoding error. Missing continuation byte in "
                           "%ld. character position:\n%s\n",
                           static_cast<long>(std::distance(src.begin(), u8)),
                           src.c_str());
          u2.h = 0xff;
          u2.l = 0xfd;
        }
        break;
      }
      case 0xe0: {  // 3-byte UTF-8 codes
        if ((*(u8 + 1) & 0xc0) == 0x80) {
          u2.h = ((*u8 & 0x0f) << 4) + ((*(u8 + 1) & 0x3f) >> 2);
          ++u8;
          if ((*(u8 + 1) & 0xc0) == 0x80) {
            u2.l = (static_cast<unsigned char>(*u8) << 6) + (*(u8 + 1) & 0x3f);
            ++u8;
          } else {
            HUNSPELL_WARNING(stderr,
                             "UTF-8 encoding error. Missing continuation byte "
                             "in %ld. character position:\n%s\n",
                             static_cast<long>(std::distance(src.begin(), u8)),
                             src.c_str());
            u2.h = 0xff;
            u2.l = 0xfd;
          }
        } else {
          HUNSPELL_WARNING(stderr,
                           "UTF-8 encoding error. Missing continuation byte in "
                           "%ld. character position:\n%s\n",
                           static_cast<long>(std::distance(src.begin(), u8)),
                           src.c_str());
          u2.h = 0xff;
          u2.l = 0xfd;
        }
        break;
      }
      case 0xf0: {  // 4 or more byte UTF-8 codes
        HUNSPELL_WARNING(stderr,
                         "This UTF-8 encoding can't convert to UTF-16:\n%s\n",
                         src.c_str());
        u2.h = 0xff;
        u2.l = 0xfd;
        dest.push_back(u2);
        return -1;
      }
    }
    dest.push_back(u2);
    ++u8;
  }

  return dest.size();
}

// strip strings into token based on single char delimiter
// acts like strsep() but only uses a delim char and not
// a delim string
// default delimiter: white space characters

char* mystrsep(char** stringp, const char delim) {
  char* mp = *stringp;
  if (*mp != '\0') {
    char* dp;
    if (delim) {
      dp = strchr(mp, delim);
    } else {
      // don't use isspace() here, the string can be in some random charset
      // that's way different than the locale's
      for (dp = mp; (*dp && *dp != ' ' && *dp != '\t'); dp++)
        ;
      if (!*dp)
        dp = NULL;
    }
    if (dp) {
      *stringp = dp + 1;
      *dp = '\0';
    } else {
      *stringp = mp + strlen(mp);
    }
    return mp;
  }
  return NULL;
}

// replaces strdup with ansi version
char* mystrdup(const char* s) {
  char* d = NULL;
  if (s) {
    size_t sl = strlen(s) + 1;
    d = (char*)malloc(sl);
    if (d) {
      memcpy(d, s, sl);
    } else {
      HUNSPELL_WARNING(stderr, "Can't allocate memory.\n");
    }
  }
  return d;
}

// strcat for limited length destination string
char* mystrcat(char* dest, const char* st, int max) {
  int len;
  int len2;
  if (dest == NULL || st == NULL)
    return dest;
  len = strlen(dest);
  len2 = strlen(st);
  if (len + len2 + 1 > max)
    return dest;
  strcpy(dest + len, st);
  return dest;
}

// remove cross-platform text line end characters
void mychomp(char* s) {
  size_t k = strlen(s);
  if ((k > 0) && ((*(s + k - 1) == '\r') || (*(s + k - 1) == '\n')))
    *(s + k - 1) = '\0';
  if ((k > 1) && (*(s + k - 2) == '\r'))
    *(s + k - 2) = '\0';
}

// break text to lines
// return number of lines
int line_tok(const char* text, char*** lines, char breakchar) {
  int linenum = 0;
  if (!text) {
    return linenum;
  }
  char* dup = mystrdup(text);
  char* p = strchr(dup, breakchar);
  while (p) {
    linenum++;
    *p = '\0';
    p++;
    p = strchr(p, breakchar);
  }
  linenum++;
  *lines = (char**)malloc(linenum * sizeof(char*));
  if (!(*lines)) {
    free(dup);
    return 0;
  }

  p = dup;
  int l = 0;
  for (int i = 0; i < linenum; i++) {
    if (*p != '\0') {
      (*lines)[l] = mystrdup(p);
      if (!(*lines)[l]) {
        for (i = 0; i < l; i++)
          free((*lines)[i]);
        free(dup);
        return 0;
      }
      l++;
    }
    p += strlen(p) + 1;
  }
  free(dup);
  if (!l) {
    free(*lines);
    *lines = NULL;
  }
  return l;
}

// uniq line in place
char* line_uniq(char* text, char breakchar) {
  char** lines;
  int linenum = line_tok(text, &lines, breakchar);
  int i;
  strcpy(text, lines[0]);
  for (i = 1; i < linenum; i++) {
    int dup = 0;
    for (int j = 0; j < i; j++) {
      if (strcmp(lines[i], lines[j]) == 0) {
        dup = 1;
        break;
      }
    }
    if (!dup) {
      if ((i > 1) || (*(lines[0]) != '\0')) {
        sprintf(text + strlen(text), "%c", breakchar);
      }
      strcat(text, lines[i]);
    }
  }
  for (i = 0; i < linenum; i++) {
    free(lines[i]);
  }
  free(lines);
  return text;
}

// uniq and boundary for compound analysis: "1\n\2\n\1" -> " ( \1 | \2 ) "
char* line_uniq_app(char** text, char breakchar) {
  if (!strchr(*text, breakchar)) {
    return *text;
  }

  char** lines;
  int i;
  int linenum = line_tok(*text, &lines, breakchar);
  int dup = 0;
  for (i = 0; i < linenum; i++) {
    for (int j = 0; j < (i - 1); j++) {
      if (strcmp(lines[i], lines[j]) == 0) {
        *(lines[i]) = '\0';
        dup++;
        break;
      }
    }
  }
  if ((linenum - dup) == 1) {
    strcpy(*text, lines[0]);
    freelist(&lines, linenum);
    return *text;
  }
  char* newtext = (char*)malloc(strlen(*text) + 2 * linenum + 3 + 1);
  if (newtext) {
    free(*text);
    *text = newtext;
  } else {
    freelist(&lines, linenum);
    return *text;
  }
  strcpy(*text, " ( ");
  for (i = 0; i < linenum; i++)
    if (*(lines[i])) {
      sprintf(*text + strlen(*text), "%s%s", lines[i], " | ");
    }
  (*text)[strlen(*text) - 2] = ')';  // " ) "
  freelist(&lines, linenum);
  return *text;
}

// append s to ends of every lines in text
std::string& strlinecat(std::string& str, const std::string& apd) {
  size_t pos = 0;
  while ((pos = str.find('\n', pos)) != std::string::npos) {
    str.insert(pos, apd);
    pos += apd.length() + 1;
  }
  str.append(apd);
  return str;
}

// morphcmp(): compare MORPH_DERI_SFX, MORPH_INFL_SFX and MORPH_TERM_SFX fields
// in the first line of the inputs
// return 0, if inputs equal
// return 1, if inputs may equal with a secondary suffix
// otherwise return -1
int morphcmp(const char* s, const char* t) {
  int se = 0;
  int te = 0;
  const char* sl;
  const char* tl;
  const char* olds;
  const char* oldt;
  if (!s || !t)
    return 1;
  olds = s;
  sl = strchr(s, '\n');
  s = strstr(s, MORPH_DERI_SFX);
  if (!s || (sl && sl < s))
    s = strstr(olds, MORPH_INFL_SFX);
  if (!s || (sl && sl < s)) {
    s = strstr(olds, MORPH_TERM_SFX);
    olds = NULL;
  }
  oldt = t;
  tl = strchr(t, '\n');
  t = strstr(t, MORPH_DERI_SFX);
  if (!t || (tl && tl < t))
    t = strstr(oldt, MORPH_INFL_SFX);
  if (!t || (tl && tl < t)) {
    t = strstr(oldt, MORPH_TERM_SFX);
    oldt = NULL;
  }
  while (s && t && (!sl || sl > s) && (!tl || tl > t)) {
    s += MORPH_TAG_LEN;
    t += MORPH_TAG_LEN;
    se = 0;
    te = 0;
    while ((*s == *t) && !se && !te) {
      s++;
      t++;
      switch (*s) {
        case ' ':
        case '\n':
        case '\t':
        case '\0':
          se = 1;
      }
      switch (*t) {
        case ' ':
        case '\n':
        case '\t':
        case '\0':
          te = 1;
      }
    }
    if (!se || !te) {
      // not terminal suffix difference
      if (olds)
        return -1;
      return 1;
    }
    olds = s;
    s = strstr(s, MORPH_DERI_SFX);
    if (!s || (sl && sl < s))
      s = strstr(olds, MORPH_INFL_SFX);
    if (!s || (sl && sl < s)) {
      s = strstr(olds, MORPH_TERM_SFX);
      olds = NULL;
    }
    oldt = t;
    t = strstr(t, MORPH_DERI_SFX);
    if (!t || (tl && tl < t))
      t = strstr(oldt, MORPH_INFL_SFX);
    if (!t || (tl && tl < t)) {
      t = strstr(oldt, MORPH_TERM_SFX);
      oldt = NULL;
    }
  }
  if (!s && !t && se && te)
    return 0;
  return 1;
}

int get_sfxcount(const char* morph) {
  if (!morph || !*morph)
    return 0;
  int n = 0;
  const char* old = morph;
  morph = strstr(morph, MORPH_DERI_SFX);
  if (!morph)
    morph = strstr(old, MORPH_INFL_SFX);
  if (!morph)
    morph = strstr(old, MORPH_TERM_SFX);
  while (morph) {
    n++;
    old = morph;
    morph = strstr(morph + 1, MORPH_DERI_SFX);
    if (!morph)
      morph = strstr(old + 1, MORPH_INFL_SFX);
    if (!morph)
      morph = strstr(old + 1, MORPH_TERM_SFX);
  }
  return n;
}

int fieldlen(const char* r) {
  int n = 0;
  while (r && *r != ' ' && *r != '\t' && *r != '\0' && *r != '\n') {
    r++;
    n++;
  }
  return n;
}

bool copy_field(std::string& dest,
                const std::string& morph,
                const std::string& var) {
  if (morph.empty())
    return false;
  size_t pos = morph.find(var);
  if (pos == std::string::npos)
    return false;
  dest.clear();
  std::string beg(morph.substr(pos + MORPH_TAG_LEN, std::string::npos));

  for (size_t i = 0; i < beg.size(); ++i) {
    const char c(beg[i]);
    if (c == ' ' || c == '\t' || c == '\n')
      break;
    dest.push_back(c);
  }

  return true;
}

std::string& mystrrep(std::string& str,
                      const std::string& search,
                      const std::string& replace) {
  size_t pos = 0;
  while ((pos = str.find(search, pos)) != std::string::npos) {
    str.replace(pos, search.length(), replace);
    pos += replace.length();
  }
  return str;
}

char* mystrrep(char* word, const char* pat, const char* rep) {
  char* pos = strstr(word, pat);
  if (pos) {
    int replen = strlen(rep);
    int patlen = strlen(pat);
    while (pos) {
      if (replen < patlen) {
        char* end = word + strlen(word);
        char* next = pos + replen;
        char* prev = pos + strlen(pat);
        for (; prev < end;* next = *prev, prev++, next++)
          ;
        *next = '\0';
      } else if (replen > patlen) {
        char* end = pos + patlen;
        char* next = word + strlen(word) + replen - patlen;
        char* prev = next - replen + patlen;
        for (; prev >= end;* next = *prev, prev--, next--)
          ;
      }
      strncpy(pos, rep, replen);
      pos = strstr(word, pat);
    }
  }
  return word;
}

// reverse word
size_t reverseword(std::string& word) {
  std::reverse(word.begin(), word.end());
  return word.size();
}

// reverse word
size_t reverseword_utf(std::string& word) {
  std::vector<w_char> w;
  u8_u16(w, word);
  std::reverse(w.begin(), w.end());
  u16_u8(word, w);
  return w.size();
}

int uniqlist(char** list, int n) {
  int i;
  if (n < 2)
    return n;
  for (i = 0; i < n; i++) {
    for (int j = 0; j < i; j++) {
      if (list[j] && list[i] && (strcmp(list[j], list[i]) == 0)) {
        free(list[i]);
        list[i] = NULL;
        break;
      }
    }
  }
  int m = 1;
  for (i = 1; i < n; i++)
    if (list[i]) {
      list[m] = list[i];
      m++;
    }
  return m;
}

void freelist(char*** list, int n) {
  if (list && *list) {
    for (int i = 0; i < n; i++)
      free((*list)[i]);
    free(*list);
    *list = NULL;
  }
}

w_char upper_utf(w_char u, int langnum) {
  unsigned short idx = (u.h << 8) + u.l;
  if (idx != unicodetoupper(idx, langnum)) {
    u.h = (unsigned char)(unicodetoupper(idx, langnum) >> 8);
    u.l = (unsigned char)(unicodetoupper(idx, langnum) & 0x00FF);
  }
  return u;
}

w_char lower_utf(w_char u, int langnum) {
  unsigned short idx = (u.h << 8) + u.l;
  if (idx != unicodetolower(idx, langnum)) {
    u.h = (unsigned char)(unicodetolower(idx, langnum) >> 8);
    u.l = (unsigned char)(unicodetolower(idx, langnum) & 0x00FF);
  }
  return u;
}

std::vector<w_char>& mkallsmall_utf(std::vector<w_char>& u,
                                    int langnum) {
  for (size_t i = 0; i < u.size(); ++i) {
    unsigned short idx = (u[i].h << 8) + u[i].l;
    if (idx != unicodetolower(idx, langnum)) {
      u[i].h = (unsigned char)(unicodetolower(idx, langnum) >> 8);
      u[i].l = (unsigned char)(unicodetolower(idx, langnum) & 0x00FF);
    }
  }
  return u;
}

std::vector<w_char>& mkallcap_utf(std::vector<w_char>& u, int langnum) {
  for (size_t i = 0; i < u.size(); i++) {
    unsigned short idx = (u[i].h << 8) + u[i].l;
    if (idx != unicodetoupper(idx, langnum)) {
      u[i].h = (unsigned char)(unicodetoupper(idx, langnum) >> 8);
      u[i].l = (unsigned char)(unicodetoupper(idx, langnum) & 0x00FF);
    }
  }
  return u;
}

std::vector<w_char>& mkinitcap_utf(std::vector<w_char>& u, int langnum) {
  if (!u.empty()) {
    unsigned short idx = (u[0].h << 8) + u[0].l;
    if (idx != unicodetoupper(idx, langnum)) {
      u[0].h = (unsigned char)(unicodetoupper(idx, langnum) >> 8);
      u[0].l = (unsigned char)(unicodetoupper(idx, langnum) & 0x00FF);
    }
  }
  return u;
}

std::vector<w_char>& mkinitsmall_utf(std::vector<w_char>& u, int langnum) {
  if (!u.empty()) {
    unsigned short idx = (u[0].h << 8) + u[0].l;
    if (idx != unicodetolower(idx, langnum)) {
      u[0].h = (unsigned char)(unicodetolower(idx, langnum) >> 8);
      u[0].l = (unsigned char)(unicodetolower(idx, langnum) & 0x00FF);
    }
  }
  return u;
}

// conversion function for protected memory
void store_pointer(char* dest, char* source) {
  memcpy(dest, &source, sizeof(char*));
}

// conversion function for protected memory
char* get_stored_pointer(const char* s) {
  char* p;
  memcpy(&p, s, sizeof(char*));
  return p;
}

/* map to lower case and remove non alphanumeric chars */
static void toAsciiLowerAndRemoveNonAlphanumeric(const char* pName,
                                                 char* pBuf) {
  while (*pName) {
    /* A-Z */
    if ((*pName >= 0x41) && (*pName <= 0x5A)) {
      *pBuf = (*pName) + 0x20; /* toAsciiLower */
      pBuf++;
    }
    /* a-z, 0-9 */
    else if (((*pName >= 0x61) && (*pName <= 0x7A)) ||
             ((*pName >= 0x30) && (*pName <= 0x39))) {
      *pBuf = *pName;
      pBuf++;
    }

    pName++;
  }

  *pBuf = '\0';
}


// language to encoding default map

struct lang_map {
  const char* lang;
  int num;
};

static struct lang_map lang2enc[] =
    {{"ar", LANG_ar},    {"az", LANG_az},
     {"az_AZ", LANG_az},  // for back-compatibility
     {"bg", LANG_bg},    {"ca", LANG_ca},
     {"cs", LANG_cs},    {"da", LANG_da},
     {"de", LANG_de},    {"el", LANG_el},
     {"en", LANG_en},    {"es", LANG_es},
     {"eu", LANG_eu},    {"gl", LANG_gl},
     {"fr", LANG_fr},    {"hr", LANG_hr},
     {"hu", LANG_hu},    {"hu_HU", LANG_hu},  // for back-compatibility
     {"it", LANG_it},    {"la", LANG_la},
     {"lv", LANG_lv},    {"nl", LANG_nl},
     {"pl", LANG_pl},    {"pt", LANG_pt},
     {"sv", LANG_sv},    {"tr", LANG_tr},
     {"tr_TR", LANG_tr},  // for back-compatibility
     {"ru", LANG_ru},    {"uk", LANG_uk}};

int get_lang_num(const char* lang) {
  int n = sizeof(lang2enc) / sizeof(lang2enc[0]);
  for (int i = 0; i < n; i++) {
    if (strcmp(lang, lang2enc[i].lang) == 0) {
      return lang2enc[i].num;
    }
  }
  return LANG_xx;
}

int initialize_utf_tbl() {
  utf_tbl_count++;
  if (utf_tbl)
    return 0;
  utf_tbl = (unicode_info2*)malloc(CONTSIZE * sizeof(unicode_info2));
  if (utf_tbl) {
    size_t j;
    for (j = 0; j < CONTSIZE; j++) {
      utf_tbl[j].cletter = 0;
      utf_tbl[j].clower = (unsigned short)j;
      utf_tbl[j].cupper = (unsigned short)j;
    }
    for (j = 0; j < UTF_LST_LEN; j++) {
      utf_tbl[utf_lst[j].c].cletter = 1;
      utf_tbl[utf_lst[j].c].clower = utf_lst[j].clower;
      utf_tbl[utf_lst[j].c].cupper = utf_lst[j].cupper;
    }
  } else
    return 1;
  return 0;
}

void free_utf_tbl() {
  if (utf_tbl_count > 0)
    utf_tbl_count--;
  if (utf_tbl && (utf_tbl_count == 0)) {
    free(utf_tbl);
    utf_tbl = NULL;
  }
}

unsigned short unicodetoupper(unsigned short c, int langnum)
{
    // In Azeri and Turkish, I and i dictinct letters:
    // There are a dotless lower case i pair of upper `I',
    // and an upper I with dot pair of lower `i'.
    if (c == 0x0069 && ((langnum == LANG_az) || (langnum == LANG_tr)))
        return 0x0130;
    return (utf_tbl) ? utf_tbl[c].cupper : c;
}

unsigned short unicodetolower(unsigned short c, int langnum)
{
    // In Azeri and Turkish, I and i dictinct letters:
    // There are a dotless lower case i pair of upper `I',
    // and an upper I with dot pair of lower `i'.
    if (c == 0x0049 && ((langnum == LANG_az) || (langnum == LANG_tr)))
        return 0x0131;
    return (utf_tbl) ? utf_tbl[c].clower : c;
}

int unicodeisalpha(unsigned short c)
{
    return (utf_tbl) ? utf_tbl[c].cletter : 0;
}

int get_captype_utf8(const std::vector<w_char>& word, int langnum) {
  // now determine the capitalization type of the first nl letters
  size_t ncap = 0;
  size_t nneutral = 0;
  size_t firstcap = 0;
  for (size_t i = 0; i < word.size(); ++i) {
    unsigned short idx = (word[i].h << 8) + word[i].l;
    if (idx != unicodetolower(idx, langnum))
      ncap++;
    if (unicodetoupper(idx, langnum) == unicodetolower(idx, langnum))
      nneutral++;
  }
  if (ncap) {
    unsigned short idx = (word[0].h << 8) + word[0].l;
    firstcap = (idx != unicodetolower(idx, langnum));
  }

  // now finally set the captype
  if (ncap == 0) {
    return NOCAP;
  } else if ((ncap == 1) && firstcap) {
    return INITCAP;
  } else if ((ncap == word.size()) || ((ncap + nneutral) == word.size())) {
    return ALLCAP;
  } else if ((ncap > 1) && firstcap) {
    return HUHINITCAP;
  }
  return HUHCAP;
}

// strip all ignored characters in the string
size_t remove_ignored_chars_utf(std::string& word,
                                const std::vector<w_char>& ignored_chars) {
  std::vector<w_char> w;
  std::vector<w_char> w2;
  u8_u16(w, word);

  for (size_t i = 0; i < w.size(); ++i) {
    if (!std::binary_search(ignored_chars.begin(),
                            ignored_chars.end(),
                            w[i])) {
      w2.push_back(w[i]);
    }
  }

  u16_u8(word, w2);
  return w2.size();
}

namespace {
class is_any_of {
 public:
  is_any_of(const std::string& in) : chars(in) {}

  bool operator()(char c) { return chars.find(c) != std::string::npos; }

 private:
  const std::string& chars;
};
}

// strip all ignored characters in the string
size_t remove_ignored_chars(std::string& word,
                            const std::string& ignored_chars) {
  word.erase(
      std::remove_if(word.begin(), word.end(), is_any_of(ignored_chars)),
      word.end());
  return word.size();
}

int parse_string(char* line, char** out, int ln) {
  char* tp = line;
  char* piece;
  int i = 0;
  int np = 0;
  if (*out) {
    HUNSPELL_WARNING(stderr, "error: line %d: multiple definitions\n", ln);
    return 1;
  }
  piece = mystrsep(&tp, 0);
  while (piece) {
    if (*piece != '\0') {
      switch (i) {
        case 0: {
          np++;
          break;
        }
        case 1: {
          *out = mystrdup(piece);
          if (!*out)
            return 1;
          np++;
          break;
        }
        default:
          break;
      }
      i++;
    }
    // free(piece);
    piece = mystrsep(&tp, 0);
  }
  if (np != 2) {
    HUNSPELL_WARNING(stderr, "error: line %d: missing data\n", ln);
    return 1;
  }
  return 0;
}

bool parse_array(char* line,
                 char** out,
                 std::vector<w_char>& out_utf16,
                 int ln) {
  if (parse_string(line, out, ln))
    return false;
    u8_u16(out_utf16, *out);
    std::sort(out_utf16.begin(), out_utf16.end());
  return true;
}
