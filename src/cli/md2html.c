/*
 * MD4C: Markdown parser for C
 * (https://github.com/mity/md4c)
 *
 * Copyright (c) 2016-2026 Martin Mitáš
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/*
 * Minuta's md2html: md4c's md2html front end plus --codepage and
 * --out-codepage, so DOS (CP850) and Windows (CP1252) files convert without
 * re-encoding them first.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "md4c-html.h"
#include "cmdline.h"
#include "encoding.h"



/* Global options. */
static unsigned parser_flags = 0;
#ifndef MD4C_USE_ASCII
    static unsigned renderer_flags = MD_HTML_FLAG_DEBUG | MD_HTML_FLAG_SKIP_UTF8_BOM;
#else
    static unsigned renderer_flags = MD_HTML_FLAG_DEBUG;
#endif
static int want_fullhtml = 0;
static int want_xhtml = 0;
static int want_stat = 0;
static int want_replay_fuzz = 0;

static const char* html_title = NULL;
static const char* css_path = NULL;

#ifdef __DOS__
    static enum encoding input_encoding = ENCODING_CP850;
#else
    static enum encoding input_encoding = ENCODING_CP1252;
#endif
static enum encoding output_encoding = ENCODING_UTF8;


/*********************************
 ***  Simple grow-able buffer  ***
 *********************************/

/* We render to a memory buffer instead of directly outputting the rendered
 * documents, as this allows using this utility for evaluating performance
 * of MD4C (--stat option). This allows us to measure just time of the parser,
 * without the I/O.
 */

struct membuffer {
    char* data;
    size_t asize;
    size_t size;
};

static void
membuf_init(struct membuffer* buf, MD_SIZE new_asize)
{
    buf->size = 0;
    buf->asize = new_asize;
    buf->data = malloc(buf->asize);
    if(buf->data == NULL) {
        fprintf(stderr, "membuf_init: malloc() failed.\n");
        exit(EXIT_FAILURE);
    }
}

static void
membuf_fini(struct membuffer* buf)
{
    if(buf->data)
        free(buf->data);
}

static void
membuf_grow(struct membuffer* buf, size_t new_asize)
{
    buf->data = realloc(buf->data, new_asize);
    if(buf->data == NULL) {
        fprintf(stderr, "membuf_grow: realloc() failed.\n");
        exit(EXIT_FAILURE);
    }
    buf->asize = new_asize;
}

static void
membuf_append(struct membuffer* buf, const char* data, MD_SIZE size)
{
    if(size > (size_t)-1 - buf->size) {
        fprintf(stderr, "membuf_append: size overflow.\n");
        exit(EXIT_FAILURE);
    }
    if(buf->asize < buf->size + size) {
        size_t new_asize = buf->size + buf->size / 2 + size;
        if(new_asize < buf->size + size)
            new_asize = buf->size + size;
        membuf_grow(buf, new_asize);
    }
    memcpy(buf->data + buf->size, data, size);
    buf->size += size;
}


/**********************
 ***  Main program  ***
 **********************/

static void
process_output(const MD_CHAR* text, MD_SIZE size, void* userdata)
{
    membuf_append((struct membuffer*) userdata, text, size);
}

static int
process_file(const char* in_path, FILE* in, FILE* out)
{
    size_t n;
    struct membuffer buf_in = {0};
    struct membuffer buf_out = {0};
    int ret = -1;
    clock_t t0, t1;
    unsigned p_flags = parser_flags;
    unsigned r_flags = renderer_flags;

    membuf_init(&buf_in, 32 * 1024);

    /* Read the input file into a buffer. */
    while(1) {
        if(buf_in.size >= buf_in.asize)
            membuf_grow(&buf_in, buf_in.asize + buf_in.asize / 2);

        n = fread(buf_in.data + buf_in.size, 1, buf_in.asize - buf_in.size, in);
        if(n == 0)
            break;
        buf_in.size += n;
    }

    /* Input size is good estimation of output size. Add some more reserve to
     * deal with the HTML header/footer and tags. */
    membuf_init(&buf_out, (MD_SIZE)(buf_in.size + buf_in.size/8 + 64));

    /* Special mode for reproducing a test case found with a fuzzing tool.
     * We assume file the same file format as produced by the fuzzer implemented
     * in test/fuzzers/fuzz-mdhtml.c. */
    if(want_replay_fuzz) {
        if(buf_in.size < 2 * sizeof(unsigned)) {
            fprintf(stderr, "File %s isn't valid fuzz test case.\n", in_path);
            ret = -1;
            goto out;
        }

        /* Override parser and renderer flags with those from the test case. */
        p_flags = ((unsigned*)buf_in.data)[0];
        r_flags = ((unsigned*)buf_in.data)[1] | MD_HTML_FLAG_DEBUG;

        /* And get rid of them from the text input to the parser. */
        memmove(buf_in.data, buf_in.data + 2 * sizeof(unsigned),
                    buf_in.size - 2 * sizeof(unsigned));
        buf_in.size -= 2 * sizeof(unsigned);
    }

    /* md4c works on UTF-8. A BOM always means UTF-8; anything else is taken
     * to be in the input code page. */
    {
        size_t bom = utf8_bom_length(buf_in.data, buf_in.size);
        enum encoding source = bom > 0 ? ENCODING_UTF8 : input_encoding;

        if(source != ENCODING_UTF8) {
            size_t converted_size;
            char* converted = encoding_to_utf8(buf_in.data, buf_in.size, source, &converted_size);

            if(converted == NULL) {
                fprintf(stderr, "Out of memory.\n");
                goto out;
            }
            membuf_fini(&buf_in);
            buf_in.data = converted;
            buf_in.size = converted_size;
            buf_in.asize = converted_size + 1;
        }
    }

    /* Parse the document. This shall call our callbacks provided via the
     * md_renderer_t structure. */
    t0 = clock();

    ret = md_html(buf_in.data, (MD_SIZE)buf_in.size, process_output,
                (void*) &buf_out, p_flags, r_flags);

    t1 = clock();
    if(ret != 0) {
        fprintf(stderr, "Parsing failed.\n");
        goto out;
    }

    if(output_encoding != ENCODING_UTF8) {
        size_t converted_size;
        char* converted = encoding_from_utf8(buf_out.data, buf_out.size, output_encoding,
                                             UNMAPPABLE_HTML_ENTITY, &converted_size);

        if(converted == NULL) {
            fprintf(stderr, "Out of memory.\n");
            ret = -1;
            goto out;
        }
        membuf_fini(&buf_out);
        buf_out.data = converted;
        buf_out.size = converted_size;
        buf_out.asize = converted_size + 1;
    }

    /* Write down the document in the HTML format. */
    if(want_fullhtml) {
        if(want_xhtml) {
            fprintf(out, "<?xml version=\"1.0\" encoding=\"%s\"?>\n", encoding_charset(output_encoding));
            fprintf(out, "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" "
                            "\"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">\n");
            fprintf(out, "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n");
        } else {
            fprintf(out, "<!DOCTYPE html>\n");
            fprintf(out, "<html>\n");
        }
        fprintf(out, "<head>\n");
        fprintf(out, "<title>%s</title>\n", html_title ? html_title : "");
        fprintf(out, "<meta name=\"generator\" content=\"md2html\"%s>\n", want_xhtml ? " /" : "");
        /* The http-equiv form, because IE 4/5 ignore <meta charset>. */
        fprintf(out, "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=%s\"%s>\n",
                encoding_charset(output_encoding), want_xhtml ? " /" : "");
        if(css_path != NULL) {
            fprintf(out, "<link rel=\"stylesheet\" href=\"%s\"%s>\n", css_path, want_xhtml ? " /" : "");
        }
        fprintf(out, "</head>\n");
        fprintf(out, "<body>\n");
    }

    fwrite(buf_out.data, 1, buf_out.size, out);

    if(want_fullhtml) {
        fprintf(out, "</body>\n");
        fprintf(out, "</html>\n");
    }

    if(want_stat) {
        if(t0 != (clock_t)-1  &&  t1 != (clock_t)-1) {
            double elapsed = (double)(t1 - t0) / CLOCKS_PER_SEC;
            if (elapsed < 1)
                fprintf(stderr, "Time spent on parsing: %7.2f ms.\n", elapsed*1e3);
            else
                fprintf(stderr, "Time spent on parsing: %6.3f s.\n", elapsed);
        }
    }

    /* Success if we have reached here. */
    ret = 0;

out:
    membuf_fini(&buf_in);
    membuf_fini(&buf_out);

    return ret;
}


static const CMDLINE_OPTION cmdline_options[] = {
    { 'o', "output",                        'o', CMDLINE_OPTFLAG_REQUIREDARG },
    { 'f', "full-html",                     'f', 0 },
    { 'x', "xhtml",                         'x', 0 },
    { 's', "stat",                          's', 0 },
    { 'h', "help",                          'h', 0 },
    { 'v', "version",                       'v', 0 },

    {  0,  "html-title",                    '1', CMDLINE_OPTFLAG_REQUIREDARG },
    {  0,  "html-css",                      '2', CMDLINE_OPTFLAG_REQUIREDARG },
    {  0,  "codepage",                      'p', CMDLINE_OPTFLAG_REQUIREDARG },
    {  0,  "out-codepage",                  'q', CMDLINE_OPTFLAG_REQUIREDARG },

    {  0,  "commonmark",                    'c', 0 },
    {  0,  "github",                        'g', 0 },
    {  0,  "gfm",                           'g', 0 },

    {  0,  "fadmonitions",                  'D', 0 },
    {  0,  "fcollapse-whitespace",          'W', 0 },
    {  0,  "ffootnotes",                    'N', 0 },
    {  0,  "fhard-soft-breaks",             'B', 0 },
    {  0,  "fhighlight",                    'M', 0 },
    {  0,  "finsert",                       'C', 0 },
    {  0,  "flatex-math",                   'L', 0 },
    {  0,  "fpermissive-atx-headers",       'A', 0 },
    {  0,  "fpermissive-autolinks",         'V', 0 },
    {  0,  "fpermissive-email-autolinks",   '@', 0 },
    {  0,  "fpermissive-url-autolinks",     'U', 0 },
    {  0,  "fpermissive-www-autolinks",     '.', 0 },
    {  0,  "fpreserve-blank-lines",         'Y', 0 },
    {  0,  "fspoilers",                     'P', 0 },
    {  0,  "fstrikethrough",                'S', 0 },
    {  0,  "fsubscripts",                   '~', 0 },
    {  0,  "fsuperscripts",                 '^', 0 },
    {  0,  "ftables",                       'T', 0 },
    {  0,  "ftasklists",                    'X', 0 },
    {  0,  "funderline",                    '_', 0 },
    {  0,  "fverbatim-entities",            'E', 0 },
    {  0,  "fwiki-links",                   'K', 0 },

    {  0,  "fno-html-blocks",               'F', 0 },
    {  0,  "fno-html-spans",                'G', 0 },
    {  0,  "fno-html",                      'H', 0 },
    {  0,  "fno-indented-code",             'I', 0 },

    /* Undocumented option for replaying test cases from fuzzers. */
    {  0,  "replay-fuzz",                   'r', 0 },

    {  0,  NULL,                             0,  0 }
};

static void
usage(void)
{
    printf(
        "Usage: md2html [OPTION]... [FILE]\n"
        "Convert input FILE (or standard input) in Markdown format to HTML.\n"
        "\n"
        "General options:\n"
        "  -o  --output=FILE    Output file (default is standard output)\n"
        "      --codepage=CP    Input code page: utf8, 1252 or 850 (default %s);\n"
        "                       input with a UTF-8 BOM is always read as UTF-8\n"
        "      --out-codepage=CP\n"
        "                       Output code page (default utf8); characters it\n"
        "                       cannot represent become &#NNNN; references\n"
        "  -f, --full-html      Generate full HTML document, including header\n"
        "  -x, --xhtml          Generate XHTML instead of HTML\n"
        "  -s, --stat           Measure time of input parsing\n"
        "  -h, --help           Display this help and exit\n"
        "  -v, --version        Display version and exit\n"
        "\n"
        "Markdown dialect options:\n"
        "(note these are equivalent to some combinations of the flags below)\n"
        "      --commonmark     CommonMark (this is default)\n"
        "      --gfm            Github Flavored Markdown\n"
        "      --github         Same as --gfm\n"
        "\n"
        "Markdown extension options:\n"
        "      --fadmonitions   Enable admonitions\n"
        "      --fcollapse-whitespace\n"
        "                       Collapse non-trivial whitespace\n"
        "      --ffootnotes     Enable footnote references ([^label])\n"
        "      --fhard-soft-breaks\n"
        "                       Force all soft breaks to act as hard breaks\n"
        "      --fhighlight     Enable highlight spans (==text==)\n"
        "      --finsert        Enable insert spans (++text++)\n"
        "      --flatex-math    Enable LaTeX style mathematics spans\n"
        "      --fpermissive-atx-headers\n"
        "                       Allow ATX headers without delimiting space\n"
        "      --fpermissive-email-autolinks  \n"
        "                       Allow e-mail autolinks without '<', '>' and 'mailto:'\n"
        "      --fpermissive-url-autolinks\n"
        "                       Allow URL autolinks without '<', '>'\n"
        "      --fpermissive-www-autolinks\n"
        "                       Allow WWW autolinks without any scheme (e.g. 'www.example.com')\n"
        "      --fpermissive-autolinks\n"
        "                       Same as --fpermissive-email-autolinks --fpermissive-url-autolinks\n"
        "                       --fpermissive-www-autolinks\n"
        "      --fpreserve-blank-lines\n"
        "                       Render extra blank lines between blocks as <br>\n"
        "      --fspoilers      Enable spoiler spans (||hidden text||)\n"
        "      --fstrikethrough Enable strike-through spans (~~text~~)\n"
        "      --fsubscripts    Enable subscript spans (~text~)\n"
        "      --fsuperscripts  Enable superscript spans (^text^)\n"
        "      --ftables        Enable tables\n"
        "      --ftasklists     Enable task lists\n"
        "      --funderline     Enable underline spans\n"
        "      --fwiki-links    Enable wiki links\n"
        "\n"
        "Markdown suppression options:\n"
        "      --fno-html-blocks\n"
        "                       Disable raw HTML blocks\n"
        "      --fno-html-spans\n"
        "                       Disable raw HTML spans\n"
        "      --fno-html       Same as --fno-html-blocks --fno-html-spans\n"
        "      --fno-indented-code\n"
        "                       Disable indented code blocks\n"
        "\n"
        "HTML generator options:\n"
        "      --fverbatim-entities\n"
        "                       Do not translate entities\n"
        "      --html-title=TITLE Sets the title of the document\n"
        "      --html-css=URL   In full HTML or XHTML mode add a css link\n"
        "\n",
#ifdef __DOS__
        "850"
#else
        "1252"
#endif
    );
}

static void
version(void)
{
    printf("%d.%d.%d\n", MD_VERSION_MAJOR, MD_VERSION_MINOR, MD_VERSION_RELEASE);
}

static const char* input_path = NULL;
static const char* output_path = NULL;

static int
cmdline_callback(int opt, char const* value, void* data)
{
    (void) data;   /* unused parameter */

    switch(opt) {
        case 0:
            if(input_path) {
                fprintf(stderr, "Too many arguments. Only one input file can be specified.\n");
                fprintf(stderr, "Use --help for more info.\n");
                exit(EXIT_FAILURE);
            }
            input_path = value;
            break;

        case 'o':   output_path = value; break;
        case 'f':   want_fullhtml = 1; break;
        case 'x':   want_xhtml = 1; renderer_flags |= MD_HTML_FLAG_XHTML; break;
        case 's':   want_stat = 1; break;
        case 'r':   want_replay_fuzz = 1; break;
        case 'h':   usage(); exit(EXIT_SUCCESS); break;
        case 'v':   version(); exit(EXIT_SUCCESS); break;

        case '1':   html_title = value; break;
        case '2':   css_path = value; break;
        case 'p':
        case 'q':
            if(encoding_from_name(value, opt == 'p' ? &input_encoding : &output_encoding) != 0) {
                fprintf(stderr, "Unknown code page: %s (use utf8, 1252 or 850).\n", value);
                exit(EXIT_FAILURE);
            }
            break;

        case 'c':   parser_flags |= MD_DIALECT_COMMONMARK; break;
        case 'g':   parser_flags |= MD_DIALECT_GITHUB; break;

        case 'D':   parser_flags |= MD_FLAG_ADMONITIONS; break;
        case 'E':   renderer_flags |= MD_HTML_FLAG_VERBATIM_ENTITIES; break;
        case 'M':   parser_flags |= MD_FLAG_HIGHLIGHT; break;
        case 'C':   parser_flags |= MD_FLAG_INSERT; break;
        case 'A':   parser_flags |= MD_FLAG_PERMISSIVEATXHEADERS; break;
        case 'I':   parser_flags |= MD_FLAG_NOINDENTEDCODEBLOCKS; break;
        case 'F':   parser_flags |= MD_FLAG_NOHTMLBLOCKS; break;
        case 'G':   parser_flags |= MD_FLAG_NOHTMLSPANS; break;
        case 'H':   parser_flags |= MD_FLAG_NOHTML; break;
        case 'W':   parser_flags |= MD_FLAG_COLLAPSEWHITESPACE; break;
        case 'U':   parser_flags |= MD_FLAG_PERMISSIVEURLAUTOLINKS; break;
        case '.':   parser_flags |= MD_FLAG_PERMISSIVEWWWAUTOLINKS; break;
        case '@':   parser_flags |= MD_FLAG_PERMISSIVEEMAILAUTOLINKS; break;
        case 'V':   parser_flags |= MD_FLAG_PERMISSIVEAUTOLINKS; break;
        case 'T':   parser_flags |= MD_FLAG_TABLES; break;
        case 'P':   parser_flags |= MD_FLAG_SPOILERS; break;
        case 'S':   parser_flags |= MD_FLAG_STRIKETHROUGH; break;
        case '^':   parser_flags |= MD_FLAG_SUPERSCRIPTS; break;
        case '~':   parser_flags |= MD_FLAG_SUBSCRIPTS; break;
        case 'L':   parser_flags |= MD_FLAG_LATEXMATHSPANS; break;
        case 'K':   parser_flags |= MD_FLAG_WIKILINKS; break;
        case 'X':   parser_flags |= MD_FLAG_TASKLISTS; break;
        case '_':   parser_flags |= MD_FLAG_UNDERLINE; break;
        case 'N':   parser_flags |= MD_FLAG_FOOTNOTES; break;
        case 'B':   parser_flags |= MD_FLAG_HARD_SOFT_BREAKS; break;
        case 'Y':   parser_flags |= MD_FLAG_PRESERVEBLANKLINES; break;

        default:
            fprintf(stderr, "Illegal option: %s\n", value);
            fprintf(stderr, "Use --help for more info.\n");
            exit(EXIT_FAILURE);
            break;
    }

    return 0;
}

/* md4c's command line parser only takes "--name=value" for long options;
 * also accept "--name value", which DOS users expect. */
static char**
join_long_option_values(int* argc, char** argv)
{
    char** joined = malloc((size_t)(*argc + 1) * sizeof(char*));
    int count = 0;
    int i;

    if(joined == NULL) {
        fprintf(stderr, "Out of memory.\n");
        exit(EXIT_FAILURE);
    }
    for(i = 0; i < *argc; i++) {
        const CMDLINE_OPTION* opt;
        char* arg = argv[i];

        if(i > 0  &&  i + 1 < *argc  &&  strncmp(arg, "--", 2) == 0) {
            for(opt = cmdline_options; opt->id != 0; opt++) {
                if(opt->longname != NULL  &&  (opt->flags & CMDLINE_OPTFLAG_REQUIREDARG)
                        &&  strcmp(arg + 2, opt->longname) == 0) {
                    size_t size = strlen(arg) + strlen(argv[i + 1]) + 2;
                    char* merged = malloc(size);

                    if(merged == NULL) {
                        fprintf(stderr, "Out of memory.\n");
                        exit(EXIT_FAILURE);
                    }
                    sprintf(merged, "%s=%s", arg, argv[i + 1]);
                    arg = merged;
                    i++;
                    break;
                }
            }
        }
        joined[count++] = arg;
    }
    joined[count] = NULL;
    *argc = count;
    return joined;
}

int
main(int argc, char** argv)
{
    FILE* in = stdin;
    FILE* out = stdout;
    int ret = 0;

    argv = join_long_option_values(&argc, argv);
    if(cmdline_read(cmdline_options, argc, argv, cmdline_callback, NULL) != 0) {
        usage();
        exit(EXIT_FAILURE);
    }

    if(input_path != NULL && strcmp(input_path, "-") != 0) {
        in = fopen(input_path, "rb");
        if(in == NULL) {
            fprintf(stderr, "Cannot open %s.\n", input_path);
            exit(EXIT_FAILURE);
        }
    }
    if(output_path != NULL && strcmp(output_path, "-") != 0) {
        out = fopen(output_path, "wt");
        if(out == NULL) {
            fprintf(stderr, "Cannot open %s.\n", output_path);
            exit(EXIT_FAILURE);
        }
    }

    ret = process_file((input_path != NULL) ? input_path : "<stdin>", in, out);
    if(in != stdin)
        fclose(in);
    if(out != stdout)
        fclose(out);

    return ret;
}
