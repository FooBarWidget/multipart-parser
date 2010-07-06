#include "MultipartParser.h"
#include "MultipartReader.h"
#include <stdio.h>

//#define TEST_PARSER

using namespace std;

#ifdef TEST_PARSER
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
#else
	void onPartBegin(const MultipartHeaders &headers, void *userData) {
		printf("onPartBegin:\n");
		MultipartHeaders::const_iterator it;
		MultipartHeaders::const_iterator end = headers.end();
		for (it = headers.begin(); it != headers.end(); it++) {
			printf("  %s = %s\n", it->first.c_str(), it->second.c_str());
		}
		printf("  aaa: %s\n", headers["aaa"].c_str());
	}
	
	void onPartData(const char *buffer, size_t size, void *userData) {
		//printf("onPartData: (%s)\n", string(buffer, size).c_str());
	}
	
	void onPartEnd(void *userData) {
		printf("onPartEnd\n");
	}
	
	void onEnd(void *userData) {
		printf("onEnd\n");
	}
#endif

int
main() {
	#ifdef TEST_PARSER
		MultipartParser parser;
		parser.onPartBegin = onPartBegin;
		parser.onHeaderField = onHeaderField;
		parser.onHeaderValue = onHeaderValue;
		parser.onPartData = onPartData;
		parser.onPartEnd = onPartEnd;
		parser.onEnd = onEnd;
	#else
		MultipartReader parser;
		parser.onPartBegin = onPartBegin;
		parser.onPartData = onPartData;
		parser.onPartEnd = onPartEnd;
		parser.onEnd = onEnd;
	#endif
	
	for (int i = 0; i < 5; i++) {
		parser.setBoundary("abcd");
		
		FILE *f = fopen("input2.txt", "rb");
		while (!parser.stopped() && !feof(f)) {
			char buf[100];
			size_t len = fread(buf, 1, sizeof(buf), f);
			size_t fed = 0;
			do {
				size_t ret = parser.feed(buf + fed, len - fed);
				fed += ret;
				//printf("accepted %d bytes\n", (int) ret);
			} while (fed < len && !parser.stopped());
		}
		printf("%s\n", parser.getErrorMessage());
		fclose(f);
	}
	return 0;
}
