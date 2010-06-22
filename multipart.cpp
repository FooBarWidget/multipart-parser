#include "MultipartParser.h"
#include <stdio.h>
using namespace std;

static void
onPartBegin(const char *buffer, size_t start, size_t end, void *userData) {
	printf("onPartBegin\n");
}

static void
onHeaderField(const char *buffer, size_t start, size_t end, void *userData) {
	printf("onHeaderField: (%s)\n", string(buffer + start, end - start).c_str());
}

static void
onHeaderValue(const char *buffer, size_t start, size_t end, void *userData) {
	printf("onHeaderValue: (%s)\n", string(buffer + start, end - start).c_str());
}

static void
onPartData(const char *buffer, size_t start, size_t end, void *userData) {
	printf("onPartData: (%s)\n", string(buffer + start, end - start).c_str());
}

static void
onPartEnd(const char *buffer, size_t start, size_t end, void *userData) {
	printf("onPartEnd\n");
}

static void
onEnd(const char *buffer, size_t start, size_t end, void *userData) {
	printf("onEnd\n");
}

int
main() {
	MultipartParser parser("abcd");
	parser.onPartBegin = onPartBegin;
	parser.onHeaderField = onHeaderField;
	parser.onHeaderValue = onHeaderValue;
	parser.onPartData = onPartData;
	parser.onPartEnd = onPartEnd;
	parser.onEnd = onEnd;
	
	while (!parser.stopped() && !feof(stdin)) {
		char buf[100];
		size_t len = fread(buf, 1, sizeof(buf), stdin);
		size_t fed = 0;
		do {
			size_t ret = parser.feed(buf + fed, len - fed);
			fed += ret;
			printf("accepted %d bytes\n", (int) ret);
		} while (fed < len && !parser.stopped());
	}
	printf("%s\n", parser.getErrorMessage());
	return 0;
}
