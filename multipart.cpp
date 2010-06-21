#include <sys/types.h>
#include <string>

class MultipartParser {
public:
	typedef void (*Callback)(const char *buffer, size_t start, size_t end, void *userData);
	
private:
	static const char CR     = 13;
	static const char LF     = 10;
	static const char SPACE  = 32;
	static const char HYPHEN = 45;
	static const char COLON  = 58;
	static const char A      = 97;
	static const char Z      = 122;
	static const size_t UNMARKED = (size_t) -1;
	
	enum State {
		ERROR,
		START,
		START_BOUNDARY,
		HEADER_FIELD_START,
		HEADER_FIELD,
		HEADER_VALUE_START,
		HEADER_VALUE,
		HEADER_VALUE_ALMOST_DONE,
		HEADERS_ALMOST_DONE,
		PART_DATA_START,
		PART_DATA,
		PART_END,
		END
	};
	
	enum Flags {
		PART_BOUNDARY = 1,
		LAST_BOUNDARY = 2
	};
	
	std::string boundary;
	char *lookbehind;
	State state;
	int flags;
	size_t index;
	size_t headerFieldMark;
	size_t headerValueMark;
	size_t partDataMark;
	
	void callback(Callback cb, const char *buffer = NULL, size_t start = UNMARKED, size_t end = UNMARKED) {
		if (start != UNMARKED && start == end) {
			return;
		}
		if (cb != NULL) {
			cb(buffer, start, end, userData);
		}
	}
	
	void dataCallback(Callback cb, size_t &mark, const char *buffer, size_t i, size_t bufferLen, bool clear) {
		if (mark == UNMARKED) {
			return;
		}
		
		if (!clear) {
			callback(cb, buffer, mark, bufferLen);
		} else {
			callback(cb, buffer, mark, i);
			mark = UNMARKED;
		}
	}
	
	char lower(char c) const {
		return c | 0x20;
	}
	
	bool isBoundaryChar(char c) const {
		const char *current = boundary.c_str();
		const char *end = current + boundary.size();
		
		while (current < end) {
			if (*current == c) {
				return true;
			}
			current++;
		}
		return false;
	}
	
public:
	Callback onPartBegin;
	Callback onHeaderField;
	Callback onHeaderValue;
	Callback onPartData;
	Callback onPartEnd;
	Callback onEnd;
	void *userData;
	
	MultipartParser() {
		lookbehind = NULL;
		reset();
	}
	
	MultipartParser(const std::string &boundary) {
		lookbehind = NULL;
		setBoundary(boundary);
	}
	
	~MultipartParser() {
		delete[] lookbehind;
	}
	
	void reset() {
		delete[] lookbehind;
		state = ERROR;
		lookbehind = NULL;
		flags = 0;
		index = 0;
		headerFieldMark = UNMARKED;
		headerValueMark = UNMARKED;
		partDataMark    = UNMARKED;
		
		onPartBegin   = NULL;
		onHeaderField = NULL;
		onHeaderValue = NULL;
		onPartData    = NULL;
		onPartEnd     = NULL;
		onEnd         = NULL;
		userData      = NULL;
	}
	
	void setBoundary(const std::string &boundary) {
		reset();
		this->boundary = "\r\n--" + boundary;
		lookbehind = new char[this->boundary.size() + 8];
		state = START;
	}
	
	size_t feed(const char *buffer, size_t len) {
		State state         = this->state;
		int flags           = this->flags;
		size_t prevIndex    = this->index;
		size_t index        = this->index;
		size_t boundarySize = boundary.size();
		size_t boundaryEnd  = boundarySize - 1;
		size_t i;
		char c, cl;
		
		for (i = 0; i < len; i++) {
			c = buffer[i];
			
			switch (state) {
			case ERROR:
				return i;
			case START:
				index = 0;
				state = START_BOUNDARY;
			case START_BOUNDARY:
				if (index == boundarySize - 2) {
					if (c != CR) {
						return i;
					}
					index++;
					break;
				} else if (index - 1 == boundarySize - 2) {
					if (c != LF) {
						return i;
					}
					index = 0;
					callback(onPartBegin);
					state = HEADER_FIELD_START;
					break;
				}
				if (c != boundary[index + 2]) {
					return i;
				}
				index++;
				break;
			case HEADER_FIELD_START:
				state = HEADER_FIELD;
				headerFieldMark = i;
			case HEADER_FIELD:
				if (c == CR) {
					headerFieldMark = UNMARKED;
					state = HEADERS_ALMOST_DONE;
					break;
				}

				if (c == HYPHEN) {
					break;
				}

				if (c == COLON) {
					dataCallback(onHeaderField, headerFieldMark, buffer, i, len, true);
					state = HEADER_VALUE_START;
					break;
				}

				cl = lower(c);
				if (cl < A || cl > Z) {
					return i;
				}
				break;
			case HEADER_VALUE_START:
				if (c == SPACE) {
					break;
				}
				
				headerValueMark = i;
				state = HEADER_VALUE;
			case HEADER_VALUE:
				if (c == CR) {
					dataCallback(onHeaderValue, headerValueMark, buffer, i, len, true);
					state = HEADER_VALUE_ALMOST_DONE;
				}
				break;
			case HEADER_VALUE_ALMOST_DONE:
				if (c != LF) {
					return i;
				}
				
				state = PART_DATA_START;
				break;
			case PART_DATA_START:
				state = PART_DATA;
				partDataMark = i;
			case PART_DATA:
				prevIndex = index;
				
				if (index == 0) {
					// boyer-moore derrived algorithm to safely skip non-boundary data
					while (i + boundary.size() <= len) {
						if (isBoundaryChar(buffer[i + boundaryEnd])) {
							break;
						}
						
						i += boundary.size();
					}
					c = buffer[i];
				}
				
				if (index < boundary.size()) {
					if (boundary[index] == c) {
						if (index == 0) {
							dataCallback(onPartData, partDataMark, buffer, i, len, true);
						}
						index++;
					} else {
						index = 0;
					}
				} else if (index == boundary.size()) {
					index++;
					if (c == CR) {
						// CR = part boundary
						flags |= PART_BOUNDARY;
					} else if (c == HYPHEN) {
						// HYPHEN = end boundary
						flags |= LAST_BOUNDARY;
					} else {
						index = 0;
					}
				} else if (index - 1 == boundary.size()) {
					if (flags & PART_BOUNDARY) {
						index = 0;
						if (c == LF) {
							// unset the PART_BOUNDARY flag
							flags &= ~PART_BOUNDARY;
							callback(onPartEnd);
							callback(onPartBegin);
							state = HEADER_FIELD_START;
							break;
						}
					} else if (flags & LAST_BOUNDARY) {
						if (c == HYPHEN) {
							index++;
						} else {
							index = 0;
						}
					} else {
						index = 0;
					}
				} else if (index - 2 == boundary.size()) {
					if (c == CR) {
						index++;
					} else {
						index = 0;
					}
				} else if (index - boundary.size() == 3) {
					index = 0;
					if (c == LF) {
						callback(onPartEnd);
						callback(onEnd);
						state = END;
						break;
					}
				}
				
				if (index > 0) {
					// when matching a possible boundary, keep a lookbehind reference
					// in case it turns out to be a false lead
					lookbehind[index - 1] = c;
				} else if (prevIndex > 0) {
					// if our boundary turned out to be rubbish, the captured lookbehind
					// belongs to partData
					callback(onPartData, lookbehind, 0, prevIndex);
					prevIndex = 0;
					partDataMark = i;
				}

				break;
			default:
				return i;
			}
		}
		
		dataCallback(onHeaderField, headerFieldMark, buffer, i, len, false);
		dataCallback(onHeaderValue, headerValueMark, buffer, i, len, false);
		dataCallback(onPartData, partDataMark, buffer, i, len, false);
		
		this->index = index;
		this->state = state;
		this->flags = flags;
		
		return len;
	}
};

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
	
	while (!feof(stdin)) {
		char buf[1024 * 16];
		size_t len = fread(buf, 1, sizeof(buf), stdin);
		size_t fed = 0;
		do {
			size_t ret = parser.feed(buf + fed, len - fed);
			fed += ret;
			printf("accepted %d bytes\n", (int) ret);
		} while (fed < len);
	}
	return 0;
}
