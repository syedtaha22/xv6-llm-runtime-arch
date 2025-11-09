/**
 * @file testutil.c
 * @brief Implementation of color-coded, indented test output utilities in xv6 userland.
 *
 * @author Syed Taha
 * @date 9th November 2025
 *
 * @details
 * Provides functions for consistent colored and indented test output using a globally-set tag.
 * ANSI escape sequences are used to produce colors on the xv6 console.
 */

#include "kernel/types.h"
#include "user/user.h"
#include "testutil.h"

#define ANSI_RED     "\033[31m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_PURPLE  "\033[35m"
#define ANSI_RESET   "\033[0m"

static const char *global_tag = "TEST";

static void print_indent(int n) {
  for (int i = 0; i < n; i++)
    printf(" ");
}

static void print_colored(int level, const char *msg, const char* tag, const char *color) {
  print_indent(level);
  if (tag[0] != '\0')
    printf("%s%s   %s%s\n", color, tag, msg, ANSI_RESET);
  else 
    printf("%s%s%s\n", color, msg, ANSI_RESET);

}

void set_tag(const char *tag) {
  global_tag = tag;
}

void fail(int level, const char *msg) {
  print_colored(level, msg, "FAIL", ANSI_RED);
  exit(1);
}

void pass(int level, const char *msg) {
  print_colored(level, msg, "PASS", ANSI_GREEN);
}

void warn(int level, const char *msg) {
  print_colored(level, msg, "WARNING", ANSI_YELLOW);
}

void info(int level, const char *msg) {
  print_colored(level, msg, "", ANSI_PURPLE);
}
