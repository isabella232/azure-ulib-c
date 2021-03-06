// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. 
// See LICENSE file in the project root for full license information.

#include "az_ulib_ulog.h"
#include <stdarg.h>
#include <stdio.h>

const char* const AZ_ULIB_ULOG_REQUIRE_EQUALS_STRING = "%s requires equals %s\r\n";
const char* const AZ_ULIB_ULOG_REQUIRE_NOT_EQUALS_STRING = "%s requires not equals %s\r\n";
const char* const AZ_ULIB_ULOG_REQUIRE_NOT_NULL_STRING = "%s cannot be null\r\n";
const char* const AZ_ULIB_ULOG_REQUIRE_NULL_STRING = "%s shall be null\r\n";
const char* const AZ_ULIB_ULOG_REQUIRE_TYPE_OF_USTREAM_STRING = "ustream is not the correct type\r\n";
const char* const AZ_ULIB_ULOG_OUT_OF_MEMORY_STRING = "Not enough memory to create the %s\r\n";
const char* const AZ_ULIB_ULOG_REPORT_EXCEPTION_STRING = "%s got exception [%d]\r\n";

const char* const AZ_ULIB_ULOG_TYPE_STRING[] = { "ERROR", "INFO" };

void az_ulib_ulog_print(az_ulib_ulog_type type, const char* const format, ...) {
  char temp[AZ_ULIB_CONFIG_MAX_LOG_SIZE];
  va_list args;
  va_start(args, format);
  vsnprintf(temp, AZ_ULIB_CONFIG_MAX_LOG_SIZE, format, args);
  printf("[%s]%s", AZ_ULIB_ULOG_TYPE_STRING[type], temp);
  va_end(args);
}
