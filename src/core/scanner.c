#include "scanner.h"

#include <string.h>

/* Internal flag: the character is inside a code span, autolink or link
 * destination, so it can't be an emphasis delimiter. Cleared before
 * scan_line returns. */
#define SPAN_LITERAL 0x80

#define MAX_DELIMITERS 128

struct delimiter {
    size_t position;     /* first character of the run */
    size_t length;       /* original run length */
    size_t opener_left;  /* characters still usable as an opener (from the start) */
    char character;
    int can_open;
    int can_close;
};

void scan_state_init(struct scan_state *state)
{
    state->fence_char = 0;
    state->fence_length = 0;
}

static int is_space(char c)
{
    return c == ' ' || c == '\t';
}

static int is_ascii_punctuation(char c)
{
    return (c >= '!' && c <= '/') || (c >= ':' && c <= '@') || (c >= '[' && c <= '`')
        || (c >= '{' && c <= '~');
}

/* CP1252 punctuation and symbols above 0x7F, for the flanking rules. Letters
 * (Š, Œ, Ž, š, œ, ž, Ÿ, ª, µ, º and 0xC0-0xFF except × and ÷) are not. */
static int is_punctuation(char c)
{
    unsigned char byte = (unsigned char)c;

    if (byte < 0x80) {
        return is_ascii_punctuation(c);
    }
    switch (byte) {
    case 0x83: case 0x8A: case 0x8C: case 0x8E: case 0x9A: case 0x9C: case 0x9E: case 0x9F:
    case 0xAA: case 0xB5: case 0xBA:
        return 0;
    case 0xD7: case 0xF7:
        return 1;
    default:
        return byte < 0xC0;
    }
}

static void mark(unsigned char *styles, size_t start, size_t end, unsigned char flags)
{
    size_t i;

    for (i = start; i < end; i++) {
        styles[i] |= flags;
    }
}

static size_t skip_indent(const char *text, size_t size, size_t position, size_t limit)
{
    size_t count = 0;

    while (position < size && text[position] == ' ' && count < limit) {
        position++;
        count++;
    }
    return position;
}

/* Parses an opening fence; returns 1 and fills char/length if the line is one. */
static int parse_fence(const char *text, size_t size, char *fence_char, unsigned *fence_length,
                       size_t *after)
{
    size_t i = skip_indent(text, size, 0, 3);
    size_t run = 0;
    char c;

    if (i >= size || (text[i] != '`' && text[i] != '~')) {
        return 0;
    }
    c = text[i];
    while (i < size && text[i] == c) {
        run++;
        i++;
    }
    if (run < 3) {
        return 0;
    }
    if (c == '`' && memchr(text + i, '`', size - i) != NULL) {
        return 0;
    }
    *fence_char = c;
    *fence_length = (unsigned)run;
    *after = i;
    return 1;
}

static int is_closing_fence(const char *text, size_t size, const struct scan_state *state)
{
    char c;
    unsigned length;
    size_t after;

    if (!parse_fence(text, size, &c, &length, &after)) {
        return 0;
    }
    if (c != state->fence_char || length < state->fence_length) {
        return 0;
    }
    while (after < size && is_space(text[after])) {
        after++;
    }
    return after == size;
}

int scan_is_fence_line(const char *text, size_t size)
{
    char c;
    unsigned length;
    size_t after;

    /* A closing fence is also a valid opening fence, so one check covers both. */
    return parse_fence(text, size, &c, &length, &after);
}

void scan_state_advance(struct scan_state *state, const char *text, size_t size)
{
    char c;
    unsigned length;
    size_t after;

    if (state->fence_length > 0) {
        if (is_closing_fence(text, size, state)) {
            scan_state_init(state);
        }
    } else if (parse_fence(text, size, &c, &length, &after)) {
        state->fence_char = c;
        state->fence_length = length;
    }
}

static int is_thematic_break(const char *text, size_t size, size_t position)
{
    char c;
    int count = 0;

    position = skip_indent(text, size, position, 3);
    if (position >= size) {
        return 0;
    }
    c = text[position];
    if (c != '*' && c != '-' && c != '_') {
        return 0;
    }
    for (; position < size; position++) {
        if (text[position] == c) {
            count++;
        } else if (!is_space(text[position])) {
            return 0;
        }
    }
    return count >= 3;
}

static int is_setext_underline(const char *text, size_t size, size_t position)
{
    position = skip_indent(text, size, position, 3);
    if (position >= size || text[position] != '=') {
        return 0;
    }
    while (position < size && text[position] == '=') {
        position++;
    }
    while (position < size && is_space(text[position])) {
        position++;
    }
    return position == size;
}

/* A GFM table delimiter row such as "|---|:--:|" or "--- | ---". */
static int is_table_delimiter_row(const char *text, size_t size, size_t position)
{
    int pipes = 0;
    int dashes = 0;

    for (; position < size; position++) {
        char c = text[position];

        if (c == '|') {
            pipes++;
        } else if (c == '-') {
            dashes++;
        } else if (c != ':' && !is_space(c)) {
            return 0;
        }
    }
    return pipes > 0 && dashes > 0;
}

/* Finds the end of a code span starting with a run of `run` backticks at
 * position; returns the position of the closing run or size if none. */
static size_t find_code_span_end(const char *text, size_t size, size_t position, size_t run)
{
    size_t i = position + run;

    while (i < size) {
        if (text[i] == '`') {
            size_t start = i;

            while (i < size && text[i] == '`') {
                i++;
            }
            if (i - start == run) {
                return start;
            }
        } else {
            i++;
        }
    }
    return size;
}

/* "<scheme:...>" or "<user@host>"; returns the position after '>' or 0. */
static size_t match_autolink(const char *text, size_t size, size_t position)
{
    size_t i = position + 1;
    size_t scheme = 0;
    int has_at = 0;

    while (i < size && ((text[i] >= 'a' && text[i] <= 'z') || (text[i] >= 'A' && text[i] <= 'Z')
                        || (scheme > 0 && ((text[i] >= '0' && text[i] <= '9') || text[i] == '+'
                                           || text[i] == '.' || text[i] == '-')))) {
        i++;
        scheme++;
    }
    if (!(scheme >= 2 && i < size && text[i] == ':')) {
        i = position + 1;
    }
    for (; i < size; i++) {
        char c = text[i];

        if (c == '>') {
            if (i == position + 1) {
                return 0;
            }
            if (scheme >= 2 && text[position + 1 + scheme] == ':') {
                return i + 1;
            }
            return has_at ? i + 1 : 0;
        }
        if (c == '<' || is_space(c)) {
            return 0;
        }
        if (c == '@') {
            has_at = 1;
        }
    }
    return 0;
}

/* Finds the ']' matching the '[' at position, skipping escapes and literal
 * characters; returns size if unmatched. */
static size_t find_bracket_end(const char *text, size_t size, size_t position,
                               const unsigned char *styles)
{
    int depth = 0;
    size_t i;

    for (i = position; i < size; i++) {
        if (styles[i] & SPAN_LITERAL) {
            continue;
        }
        if (text[i] == '\\' && i + 1 < size) {
            i++;
        } else if (text[i] == '[') {
            depth++;
        } else if (text[i] == ']') {
            depth--;
            if (depth == 0) {
                return i;
            }
        }
    }
    return size;
}

/* Finds the ')' closing an inline link destination/title opened at position. */
static size_t find_destination_end(const char *text, size_t size, size_t position)
{
    int depth = 0;
    char quote = 0;
    size_t i;

    for (i = position; i < size; i++) {
        char c = text[i];

        if (c == '\\' && i + 1 < size) {
            i++;
        } else if (quote != 0) {
            if (c == quote) {
                quote = 0;
            }
        } else if (c == '"' && i > position + 1) {
            quote = c;
        } else if (c == '(') {
            depth++;
        } else if (c == ')') {
            depth--;
            if (depth == 0) {
                return i;
            }
        }
    }
    return size;
}

/* Pass 1: escapes, code spans, autolinks and link syntax. Marks their
 * syntax as SPAN_MARKER and the parts that can't hold emphasis as literal. */
static void scan_literals(const char *text, size_t start, size_t end, unsigned char *styles)
{
    size_t i = start;

    while (i < end) {
        char c = text[i];

        if (styles[i] & SPAN_LITERAL) {
            i++;
        } else if (c == '\\' && i + 1 < end && is_ascii_punctuation(text[i + 1])) {
            styles[i] |= SPAN_MARKER;
            styles[i + 1] |= SPAN_LITERAL;
            i += 2;
        } else if (c == '`') {
            size_t run = 1;
            size_t close;

            while (i + run < end && text[i + run] == '`') {
                run++;
            }
            close = find_code_span_end(text, end, i, run);
            if (close == end) {
                mark(styles, i, i + run, SPAN_LITERAL);
                i += run;
            } else {
                mark(styles, i, close + run, SPAN_CODE | SPAN_LITERAL);
                mark(styles, i, i + run, SPAN_MARKER);
                mark(styles, close, close + run, SPAN_MARKER);
                i = close + run;
            }
        } else if (c == '<') {
            size_t close = match_autolink(text, end, i);

            if (close > 0) {
                mark(styles, i, close, SPAN_MARKER | SPAN_LITERAL);
                i = close;
            } else {
                i++;
            }
        } else if (c == '[') {
            size_t bracket_end = find_bracket_end(text, end, i, styles);
            size_t open = (i > start && text[i - 1] == '!') ? i - 1 : i;

            if (bracket_end < end && bracket_end + 1 < end && text[bracket_end + 1] == '(') {
                size_t destination_end = find_destination_end(text, end, bracket_end + 1);

                if (destination_end < end) {
                    mark(styles, open, i + 1, SPAN_MARKER);
                    mark(styles, bracket_end, destination_end + 1, SPAN_MARKER);
                    mark(styles, bracket_end + 1, destination_end + 1, SPAN_LITERAL);
                }
            } else if (bracket_end < end && bracket_end + 1 < end && text[bracket_end + 1] == '[') {
                size_t reference_end = find_bracket_end(text, end, bracket_end + 1, styles);

                if (reference_end < end) {
                    mark(styles, open, i + 1, SPAN_MARKER);
                    mark(styles, bracket_end, reference_end + 1, SPAN_MARKER | SPAN_LITERAL);
                }
            }
            i++;
        } else {
            i++;
        }
    }
}

static void classify_delimiter(const char *text, size_t start, size_t end, struct delimiter *run)
{
    char before = run->position > start ? text[run->position - 1] : ' ';
    char after = run->position + run->length < end ? text[run->position + run->length] : ' ';
    int before_space = is_space(before);
    int after_space = is_space(after);
    int left_flanking = !after_space
        && (!is_punctuation(after) || before_space || is_punctuation(before));
    int right_flanking = !before_space
        && (!is_punctuation(before) || after_space || is_punctuation(after));

    if (run->character == '_') {
        run->can_open = left_flanking && (!right_flanking || is_punctuation(before));
        run->can_close = right_flanking && (!left_flanking || is_punctuation(after));
    } else {
        run->can_open = left_flanking;
        run->can_close = right_flanking;
    }
}

/* Pass 2: emphasis, strong emphasis and strikethrough, following the
 * CommonMark delimiter-run rules on a single line. */
static void scan_emphasis(const char *text, size_t start, size_t end, unsigned char *styles)
{
    struct delimiter stack[MAX_DELIMITERS];
    size_t depth = 0;
    size_t i = start;

    while (i < end) {
        struct delimiter run;
        size_t closer_left;
        size_t closer_used = 0;
        char c = text[i];

        if ((styles[i] & SPAN_LITERAL) || (c != '*' && c != '_' && c != '~')) {
            i++;
            continue;
        }
        run.position = i;
        run.character = c;
        while (i < end && text[i] == c && !(styles[i] & SPAN_LITERAL)) {
            i++;
        }
        run.length = i - run.position;
        run.opener_left = run.length;
        classify_delimiter(text, start, end, &run);
        if (c == '~' && run.length > 2) {
            continue;
        }
        closer_left = run.length;

        while (run.can_close && closer_left > 0) {
            size_t k = depth;
            size_t use;
            size_t open_end;
            unsigned char flags;
            struct delimiter *opener = NULL;

            while (k > 0) {
                struct delimiter *candidate = &stack[k - 1];

                if (candidate->character == c && candidate->opener_left > 0) {
                    int odd_match = (candidate->can_close || run.can_open)
                        && (candidate->length + run.length) % 3 == 0
                        && !(candidate->length % 3 == 0 && run.length % 3 == 0);

                    if (c == '~' ? candidate->length == run.length : !odd_match) {
                        opener = candidate;
                        break;
                    }
                }
                k--;
            }
            if (opener == NULL) {
                break;
            }
            if (c == '~') {
                use = closer_left;
                flags = SPAN_STRIKE;
            } else {
                use = (closer_left >= 2 && opener->opener_left >= 2) ? 2 : 1;
                flags = use == 2 ? SPAN_STRONG : SPAN_EMPHASIS;
            }
            open_end = opener->position + opener->opener_left;
            mark(styles, open_end - use, open_end, SPAN_MARKER);
            mark(styles, open_end, run.position + closer_used, flags);
            mark(styles, run.position + closer_used, run.position + closer_used + use, SPAN_MARKER);
            opener->opener_left -= use;
            closer_used += use;
            closer_left -= use;
            depth = opener->opener_left > 0 ? k : k - 1;
        }
        if (run.can_open && closer_left > 0 && depth < MAX_DELIMITERS) {
            run.position += closer_used;
            run.opener_left = closer_left;
            stack[depth++] = run;
        }
    }
}

static void scan_inline(const char *text, size_t start, size_t end, unsigned char *styles)
{
    scan_literals(text, start, end, styles);
    scan_emphasis(text, start, end, styles);
}

/* Returns the position after a list marker ("- ", "* ", "+ ", "1. ", "1) ")
 * at position, or position itself if there is none. */
static size_t match_list_marker(const char *text, size_t size, size_t position)
{
    size_t i = position;

    if (i < size && (text[i] == '-' || text[i] == '*' || text[i] == '+')) {
        i++;
    } else {
        size_t digits = 0;

        while (i < size && text[i] >= '0' && text[i] <= '9' && digits < 9) {
            i++;
            digits++;
        }
        if (digits == 0 || i >= size || (text[i] != '.' && text[i] != ')')) {
            return position;
        }
        i++;
    }
    if (i == size) {
        return i;
    }
    return is_space(text[i]) ? i : position;
}

void scan_line(const char *text, size_t size, struct scan_state *state, unsigned char *styles)
{
    size_t position = 0;
    size_t marker_end;
    int quote_depth = 0;
    char fence_char;
    unsigned fence_length;
    size_t after;
    size_t i;

    memset(styles, 0, size);
    if (state->fence_length > 0) {
        if (is_closing_fence(text, size, state)) {
            mark(styles, 0, size, SPAN_MARKER | SPAN_CODE);
            scan_state_init(state);
        } else {
            mark(styles, 0, size, SPAN_CODE);
        }
        return;
    }

    for (;;) {
        size_t next = skip_indent(text, size, position, 3);

        if (next >= size || text[next] != '>') {
            break;
        }
        styles[next] |= SPAN_MARKER;
        position = next + 1;
        if (position < size && is_space(text[position])) {
            position++;
        }
        quote_depth++;
    }

    if (quote_depth == 0 && parse_fence(text, size, &fence_char, &fence_length, &after)) {
        state->fence_char = fence_char;
        state->fence_length = fence_length;
        mark(styles, 0, size, SPAN_MARKER | SPAN_CODE);
        return;
    }
    if (is_thematic_break(text, size, position) || is_setext_underline(text, size, position)
        || is_table_delimiter_row(text, size, position)) {
        mark(styles, position, size, SPAN_MARKER);
        return;
    }

    i = skip_indent(text, size, position, 3);
    if (i < size && text[i] == '#') {
        size_t hashes = 0;

        while (i + hashes < size && text[i + hashes] == '#') {
            hashes++;
        }
        if (hashes <= 6 && (i + hashes == size || is_space(text[i + hashes]))) {
            size_t content_end = size;

            mark(styles, i, size, SPAN_HEADING);
            mark(styles, i, i + hashes, SPAN_MARKER);
            /* Optional closing sequence: spaces, '#'s, spaces at the end. */
            while (content_end > i + hashes && is_space(text[content_end - 1])) {
                content_end--;
            }
            if (content_end > i + hashes && text[content_end - 1] == '#') {
                size_t closing = content_end;

                while (closing > i + hashes && text[closing - 1] == '#') {
                    closing--;
                }
                if (closing == i + hashes || is_space(text[closing - 1])) {
                    mark(styles, closing, content_end, SPAN_MARKER);
                    content_end = closing;
                }
            }
            scan_inline(text, i + hashes, content_end, styles);
            for (i = 0; i < size; i++) {
                styles[i] &= (unsigned char)~SPAN_LITERAL;
            }
            return;
        }
    }

    marker_end = match_list_marker(text, size, i);
    if (marker_end != i) {
        mark(styles, i, marker_end, SPAN_MARKER);
        position = marker_end;
        while (position < size && is_space(text[position])) {
            position++;
        }
        if (position + 2 < size && text[position] == '[' && text[position + 2] == ']'
            && (text[position + 1] == ' ' || text[position + 1] == 'x' || text[position + 1] == 'X')) {
            mark(styles, position, position + 3, SPAN_MARKER | SPAN_LITERAL);
            position += 3;
        }
    }

    /* Link reference definitions ("[label]: url") are all syntax. */
    i = skip_indent(text, size, position, 3);
    if (i < size && text[i] == '[') {
        size_t close = find_bracket_end(text, size, i, styles);

        if (close + 1 < size && text[close + 1] == ':') {
            mark(styles, i, size, SPAN_MARKER);
            return;
        }
    }

    scan_inline(text, position, size, styles);
    /* Table pipes, on lines that look like table rows. */
    i = skip_indent(text, size, position, 3);
    if (i < size && text[i] == '|') {
        for (; i < size; i++) {
            if (text[i] == '|' && !(styles[i] & SPAN_LITERAL)
                && (i == 0 || text[i - 1] != '\\')) {
                styles[i] |= SPAN_MARKER;
            }
        }
    }
    for (i = 0; i < size; i++) {
        styles[i] &= (unsigned char)~SPAN_LITERAL;
    }
}

int scan_next_run(const unsigned char *styles, size_t size, size_t *position, struct span_run *run)
{
    size_t i = *position;

    if (i >= size) {
        return 0;
    }
    run->start = i;
    run->styles = styles[i];
    while (i < size && styles[i] == run->styles) {
        i++;
    }
    run->length = i - run->start;
    *position = i;
    return 1;
}
