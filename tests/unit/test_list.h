/* Every unit test, in run order. Included twice by registry.c. */
TEST(test_encoding_names)
TEST(test_encoding_bom)
TEST(test_encoding_utf8_validity)
TEST(test_encoding_cp1252_round_trip)
TEST(test_encoding_cp850)
TEST(test_encoding_unmappable)
TEST(test_encoding_offsets)
TEST(test_encoding_entity_worst_case)

/* RTF renderer tests (tests/unit/test_rtf.c) */
#include "test_list_rtf.h"
TEST(test_scanner_heading)
TEST(test_scanner_emphasis)
TEST(test_scanner_code_span)
TEST(test_scanner_links)
TEST(test_scanner_blocks)
TEST(test_scanner_tables)
TEST(test_scanner_fences)
TEST(test_scanner_runs)
TEST(test_document_cp1252)
TEST(test_document_utf8)
TEST(test_document_bom)
TEST(test_document_preserves_unmappable)
TEST(test_document_repeated_lines)
TEST(test_document_line_endings)
TEST(test_document_new_file)
TEST(test_stats_counts)
TEST(test_stats_reading_time)
TEST(test_stats_sentences)
TEST(test_export_utf8)
TEST(test_export_cp1252)

/* UTF-8 core tests (tests/unit/test_document_utf8.c, test_stats_utf8.c, test_styled.c) */
#include "test_list_utf8.h"
