#ifndef MINUTA_TRACE_H
#define MINUTA_TRACE_H

/* Diagnostic log (TRACE.TXT beside the exe), written only when MINUTA.INI
 * has Trace=1. For debugging on the target machine. */
void trace_open(int enabled);
void trace(const char *format, ...);

#endif
