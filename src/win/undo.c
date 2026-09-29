#include <stdlib.h>
#include <string.h>

#include "editor.h"
#include "undo.h"

/* Oldest steps are dropped beyond this much recorded text. */
#define UNDO_BYTE_LIMIT (4L * 1024 * 1024)

struct undo_step {
    long start;
    char *removed;
    long removed_size;
    char *inserted;
    long inserted_size;
    enum undo_kind kind;
};

struct undo_stack {
    struct undo_step *steps;
    size_t count;
    size_t capacity;
};

static struct undo_stack undo_steps;
static struct undo_stack redo_steps;
static long recorded_bytes;
static int merge_allowed;

static void free_step(struct undo_step *step)
{
    recorded_bytes -= step->removed_size + step->inserted_size;
    free(step->removed);
    free(step->inserted);
}

static void clear_stack(struct undo_stack *stack)
{
    size_t i;

    for (i = 0; i < stack->count; i++) {
        free_step(&stack->steps[i]);
    }
    stack->count = 0;
}

void undo_clear(void)
{
    clear_stack(&undo_steps);
    clear_stack(&redo_steps);
    merge_allowed = 0;
}

void undo_break(void)
{
    merge_allowed = 0;
}

static char *copy_text(const char *text, long size)
{
    char *copy = malloc((size_t)size + 1);

    if (copy != NULL) {
        memcpy(copy, text, (size_t)size);
        copy[size] = '\0';
    }
    return copy;
}

static int push(struct undo_stack *stack, const struct undo_step *step)
{
    if (stack->count == stack->capacity) {
        size_t capacity = stack->capacity == 0 ? 64 : stack->capacity * 2;
        struct undo_step *grown = realloc(stack->steps, capacity * sizeof(*grown));

        if (grown == NULL) {
            return 0;
        }
        stack->steps = grown;
        stack->capacity = capacity;
    }
    stack->steps[stack->count++] = *step;
    return 1;
}

/* Appends or prepends text to one side of a step. */
static int grow_text(char **text, long *size, const char *more, long more_size, int prepend)
{
    char *grown = realloc(*text, (size_t)(*size + more_size) + 1);

    if (grown == NULL) {
        return 0;
    }
    if (prepend) {
        memmove(grown + more_size, grown, (size_t)*size);
        memcpy(grown, more, (size_t)more_size);
    } else {
        memcpy(grown + *size, more, (size_t)more_size);
    }
    *size += more_size;
    grown[*size] = '\0';
    *text = grown;
    recorded_bytes += more_size;
    return 1;
}

static int try_merge(long start, const char *removed, long removed_size, const char *inserted,
                     long inserted_size, enum undo_kind kind)
{
    struct undo_step *last;

    if (!merge_allowed || undo_steps.count == 0) {
        return 0;
    }
    last = &undo_steps.steps[undo_steps.count - 1];
    if (last->kind != kind) {
        return 0;
    }
    switch (kind) {
    case UNDO_TYPING:
        /* A space after a word starts a new step: undo goes word by word. */
        if (last->start + last->inserted_size != start || removed_size != 0
            || (inserted[0] == ' ' && last->inserted[last->inserted_size - 1] != ' ')) {
            return 0;
        }
        return grow_text(&last->inserted, &last->inserted_size, inserted, inserted_size, 0);
    case UNDO_BACKSPACE:
        if (start + removed_size != last->start || inserted_size != 0) {
            return 0;
        }
        last->start = start;
        return grow_text(&last->removed, &last->removed_size, removed, removed_size, 1);
    case UNDO_DELETE:
        if (start != last->start || inserted_size != 0) {
            return 0;
        }
        return grow_text(&last->removed, &last->removed_size, removed, removed_size, 0);
    default:
        return 0;
    }
}

static void drop_oldest(void)
{
    while (recorded_bytes > UNDO_BYTE_LIMIT && undo_steps.count > 1) {
        free_step(&undo_steps.steps[0]);
        memmove(undo_steps.steps, undo_steps.steps + 1, (undo_steps.count - 1) * sizeof(undo_steps.steps[0]));
        undo_steps.count--;
    }
}

void undo_record(long start, const char *removed, long removed_size, const char *inserted,
                 long inserted_size, enum undo_kind kind)
{
    struct undo_step step;

    clear_stack(&redo_steps);
    if (try_merge(start, removed, removed_size, inserted, inserted_size, kind)) {
        drop_oldest();
        return;
    }
    step.start = start;
    step.kind = kind;
    step.removed = copy_text(removed, removed_size);
    step.inserted = copy_text(inserted, inserted_size);
    step.removed_size = removed_size;
    step.inserted_size = inserted_size;
    if (step.removed == NULL || step.inserted == NULL || !push(&undo_steps, &step)) {
        free(step.removed);
        free(step.inserted);
        undo_clear();
        return;
    }
    recorded_bytes += removed_size + inserted_size;
    merge_allowed = kind != UNDO_OTHER;
    drop_oldest();
}

int undo_undo(void)
{
    struct undo_step step;

    if (undo_steps.count == 0) {
        return 0;
    }
    step = undo_steps.steps[--undo_steps.count];
    editor_replace(step.start, step.start + step.inserted_size, step.removed, step.removed_size);
    if (!push(&redo_steps, &step)) {
        free_step(&step);
    }
    merge_allowed = 0;
    return 1;
}

int undo_redo(void)
{
    struct undo_step step;

    if (redo_steps.count == 0) {
        return 0;
    }
    step = redo_steps.steps[--redo_steps.count];
    editor_replace(step.start, step.start + step.removed_size, step.inserted, step.inserted_size);
    if (!push(&undo_steps, &step)) {
        free_step(&step);
    }
    merge_allowed = 0;
    return 1;
}
