#define STB_DEFINE
#include "stb.h"
#include <stdarg.h>

char *skipwhite(char *p)
{
   while (*p == ' ' || *p == '\t')
      ++p;
   return p;
}

int linecount(char *p, char *curpoint)
{
   int line=1;
   while (p < curpoint) {
      if (*p == '\r' || *p == '\n') {
         p += 1 + (p[0] + p[1] == '\r' + '\n' ? 1 : 0);
         ++line;
      } else
         ++p;
   }
   return line;
}

char **parse_tag(char **p_ptr)
{
   char **tokens = NULL;
   char *o = NULL;
   char *p = *p_ptr;
   assert(p[-1] == '[');

   p = skipwhite(p);
   while (*p != ']') {
      while (!isspace(*p) && *p != ']') {
         stb_arr_push(o, *p++);

         if (*p == '"' || *p == '\'') {
            char q = *p;
            stb_arr_push(o, *p++);
            while (*p != q)
               stb_arr_push(o, *p++);
            stb_arr_push(o, *p++);
         }
      }
      stb_arr_push(o, '\0');
      stb_arr_push(tokens, strdup(o));
      stb_arr_free(o);
      p = stb_skipwhite(p);
   }

   *p_ptr = p+1;
   return tokens;
}

void free_tokens(char **tokens)
{
   int i;
   for (i=0; i < stb_arr_len(tokens); ++i)
      free(tokens[i]);
   stb_arr_free(tokens);
}

char *line_buffer;

static int last_char = 0;
void aputc(int c)
{
   if (c == '#') goto skip;

   if (c == '<') { aputc('&'); aputc('l'); aputc('t'); aputc(';'); goto skip; }
   if (c == '>') { aputc('&'); aputc('g'); aputc('t'); aputc(';'); goto skip; }

   if (!(c == ' ' && last_char == ' '))
      stb_arr_push(line_buffer, c);
  skip:
   last_char = c;
}

void aprintf(char *s, ...)
{
   int i;
   char buffer[4096];
   va_list va;
   va_start(va, s);
   vsprintf(buffer, s, va);
   va_end(va);
   for (i=0; buffer[i] != 0; ++i)
      aputc(buffer[i]);
}

void adiscard(void)
{
   stb_arr_free(line_buffer);
   last_char = 0;
}

void aflush(FILE *g)
{
   if (stb_arr_len(line_buffer) != 0) {
      stb_arr_push(line_buffer, '\0');
      fprintf(g, "%s\n", line_buffer);
   }
   stb_arr_free(line_buffer);
   last_char = 0;
}

int main(int argc, char **argv)
{
   int i, m;
   if (argc < 2) stb_fatal("Usage: hmml_to_youtube <hmmlfile>+");
   for (i=1; i < argc; ++i)
   for (m=0; m < 3; ++m) {
      int at_start = 1;
      int at_newline = 1;
      int after_timestamp = 0;
      int after_text = 0;
      int first_output = 1;
      int chat_comment = 0;
      char *file = stb_file(argv[i], NULL);
      FILE *g = fopen(stb_sprintf("%s.txt", argv[i]), "wb");
      char *p = file;

      if (file == NULL)
         stb_fatal("Couldn't read '%s'", argv[i]);

      if (m > 0)
         printf("File for %s too long, shortening%s.\n", argv[i], m == 2 ? " further" : "");

      p = skipwhite(p);
      while (*p) {
         char *token_start = p;
         switch (*p) {
            case '\n':
               at_newline = 1;
               ++p;
               break;
            case '[':
               if (at_newline) {
                  char **tokenlist;
                  at_newline = 0;
                  ++p;
                  if (*p == '/') {
                     if (!stb_prefix(p, "/video]"))
                        stb_fatal("Parse error, unexpected close tag at line %d %s", linecount(file, p), argv[i]);
                     // SUCCESS!!!
                     goto done;
                  } else if (isdigit(*p)) {
                     if (at_start) stb_fatal("Parse error, unexpected timestamp tag at start of file %s", argv[i]);
                     // timestamp
                     //if (!at_start)
                     //   aputc('\n', g);
                     //at_start = 0;
                     aflush(g);
                     while (*p != ']')
                        aputc(*p++);
                     aprintf("  ");
                     assert(*p == ']');
                     ++p; 
                     after_timestamp = 1;
                     after_text = 0;
                     break;
                  } else if (isalpha(*p)) {
                     // open tag
                     if (stb_prefix(p, "video")) {
                        if (!at_start)
                           stb_fatal("Parse error, unexpected video tag at line %d %s", linecount(file, p), argv[i]);
                        tokenlist = parse_tag(&p);
                        free_tokens(tokenlist);
                        at_start = 0;
                        break;
                     }
                  }
               }

               // handle other cases
               if (p[1] == '@' && after_timestamp) {
                  // annotate comment from user being replied to
                  int nesting_depth=1;
                  after_timestamp = 0;
                  p += 2;
                  while (nesting_depth > 0) {
                     if (*p == ']') --nesting_depth;
                     if (*p == '[') ++nesting_depth;
                     if (*p == '\n' || *p == '\r') at_newline=1;
                     ++p;
                  }
                  if (m == 1) 
                     aprintf("Chat: \"");
                  else if (m == 0)
                     aprintf("Chat comment: \"");
                  chat_comment = 1;
               } else if (p[1] == ':' && after_text) {
                  // category node, discard
                  after_timestamp = 0;
                  p += 2;
                  while (*p != ']') ++p;
                  ++p;
               } else {
                  // text node
                  int after_space=1;
                  char **tokenlist;
                  int nesting_depth=1;
                  if (!after_timestamp) {
                     if (stb_prefix(p, "[quote")) {
                        while (*p != ']')
                           ++p;
                        p = skipwhite(p+1);
                        adiscard();
                        continue;
                     }
                  }
                  after_timestamp = 0;
                  if (at_start)
                     stb_fatal("Parse error, unexpected tag outside [video] block at line %d %s", linecount(file,p), argv[i]);
                  ++p;

                  if (*p == '@') {
                     ++p;
                     while (isalnum(*p))
                        ++p;
                     while (!isspace(*p) && *p != ']' && *p != '[')
                        ++p;
                     while (*p == ' ' || *p == '\t')
                        ++p;
                  }

                  while (*p != ']') {
                     if (p[0] == '\\') {
                        ++p;
                        aputc(*p++);
                     } if (*p == '[') {
                        int j;
                        ++p;
                        tokenlist = parse_tag(&p);
                        after_space = 0;
                        if (tokenlist[0][0] == ':' || tokenlist[0][0] == '@' || tokenlist[0][0] == '~') {
                           if (stb_arr_len(tokenlist) == 1)
                              stb_fatal("Name/tag token lacks readable text in line %d %s", linecount(file,p), argv[i]);
                           for (j=1; j < stb_arr_len(tokenlist); ++j) {
                              aprintf("%s", tokenlist[j]);
                              aputc(' ');
                           }
                        } else if (0==strcmp(tokenlist[0], "ref")) {
                           for (j=1; j < stb_arr_len(tokenlist); ++j) {
                              if (stb_prefix(tokenlist[j], "url=")) {
                                 char *q = stb_skipwhite(tokenlist[j]+4);
                                 if (*q == '\'' || *q == '\"') {
                                    char *z = strchr(q+1, *q);
                                    *z = 0;
                                    ++q;
                                 }
                                 aprintf("%s", q);
                                 break;
                              }
                           }
                        } else {
                           stb_fatal("Unknown nested markup token at line %d %s", linecount(file,p), argv[i]);
                        }
                     } else if (after_space && p[0] == ':') {
                        if (p[1] == '"' || p[1] == '\'') {
                           char q = p[1];
                           p += 2;
                           while (*p != q)
                              aputc(*p++);
                        } else {
                           ++p;
                           while (!isspace(*p) && *p != ']' && *p != '[')
                              aputc(*p++);
                        }
                     } else if (after_space && p[0] == '@' && isalnum(p[1])) {
                        after_space = 0;
                        ++p;
                     } else {
                        after_space = isspace(*p);
                        if (*p != '\n' && *p != '\r')
                           aputc(*p);
                        ++p;
                     }
                  }
                  ++p;
                  if (chat_comment)
                     if (m < 2)
                        aputc('"');
                     else
                        adiscard();
                  chat_comment = 0;
                  after_text = 1;
               }
               break;
            default:
               at_newline = 0;
               stb_fatal("Failed to parse token at line %d %s", linecount(file,p), argv[i]);
         }
         p = skipwhite(p);
      }
     done:
      aflush(g);
      free(file);
      //aputc('\n');
      if (ftell(g) <= 5000) {
         char *credits = "\nAnnotated by Miblo - https://handmade.network/m/Miblo";
         if (ftell(g) + strlen(credits) <= 5000)
            fputs(credits, g);
         fclose(g);
         break;
      } else {
         fclose(g);
      }
   }
   return 0;
}