/**
 * Copyright 2019 Wyres
 * Licensed under the Apache License, Version 2.0 (the "License"); 
 * you may not use this file except in compliance with the License. 
 * You may obtain a copy of the License at
 *    http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, 
 * software distributed under the License is distributed on 
 * an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, 
 * either express or implied. See the License for the specific 
 * language governing permissions and limitations under the License.
*/
#ifndef H_AT_PROCESS_H
#define H_AT_PROCESS_H

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

// Externals of the AT command processing module
// process a input line of text, which was terminated by \r\n, and is terminated by \0
// Output will be send to the given tx fn for processing
void at_process_input(char* data, UART_TX_FN_T source_txfn);
// process at cmd locally only
void at_process_line(char* line, UART_TX_FN_T utx_fn);
#ifdef __cplusplus
}
#endif

#endif  /* H_AT_PROCESS_H */
