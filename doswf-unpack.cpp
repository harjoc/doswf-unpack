/*
 * Decoder for DoSWF.
 * This extracts the DefineBinary tag from argv[1] to argv[2]. To un-zlib that using python:
 * 
 * python -u -c \
 *   "import sys, zlib; sys.stdout.write(zlib.decompress(sys.stdin.read()))" \
 *   < definebinary.z > definebinary.txt
 *
 * Then trim off the first 6 bytes to get to the SWF header.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma warning(disable:4996)

#ifdef _DEBUG
#define DBGBREAK() __debugbreak()
#else
#define DBGBREAK()
#endif

#define LOG(...) fprintf(stderr, __VA_ARGS__)
#define DIE(...) (LOG(__VA_ARGS__), DBGBREAK(), exit(1), 0)

typedef unsigned char byte;

void handle_DefineBinary(FILE *fi, FILE *fo, int taglen, byte *buffer)
{
	LOG("* dumping DefineBinary section\n");

	byte *end = buffer + taglen;
	byte *ptr = buffer, *old;

	#define GET(type) ( \
			((ptr+sizeof(type) >= end) ? DIE("eof\n") : 0), \
			old = ptr, \
			ptr += sizeof(type), \
			*(type *)old)

	short tag = GET(short);
	int reserved = GET(int);

	byte block_size = GET(byte) - 1;
	byte key = GET(byte) - 1;
	int offset = GET(int) - 2;
	int length = GET(int) - 2;

	byte *data = buffer+taglen-length;
	
	for (int count = 0; count < length;) {
		for (int i = 0; i < block_size; i += 5) {
			data[count] = data[count] ^ key;
			++count;
			if (count >= length)
				break;
		}
		count = count + offset;
	}

	fwrite(data, 1, length, fo);
}

int main(int argc, char *argv[])
{
	if (argc != 3)
		DIE("syntax: UndoSWF <input.swf> <output.as>\n");

	FILE *fi = fopen(argv[1], "rb");
	if (!fi)
		DIE("can't open %s\n", argv[1]);

	FILE *fo = fopen(argv[2], "wb");
	if (!fo)
		DIE("can't create %s\n", argv[2]);

	char header[9];
	if (fread(header, 1, 9, fi) != 9)
		DIE("can't read header\n");
	if (memcmp(header, "FWS", 3))
		DIE("invalid header\n");

	int rectbits = header[8] >> 3;
	int hdrbits = 8*8 + 5+rectbits*4;
	int hdrbytes = (hdrbits+7)/8 + 4;

	LOG("header size: %d\n", hdrbytes);
	if (fseek(fi, hdrbytes-9, SEEK_CUR) < 0)
		DIE("can't skip %d bytes\n", hdrbytes-9);

	for (;;) {
		unsigned short hdr;
		if (fread(&hdr, 1, 2, fi) != 2)
			DIE("can't read tag hdr\n");

		if (hdr == 0) break;

		int taglen  = hdr & 0x3f;
		int tagtype = hdr >> 6;
		
		bool longtag = taglen == 0x3f;
		if (longtag) {
			if (fread(&taglen, 1, 4, fi) != 4)
				DIE("can't read tag len\n");
		}

		LOG("tag type=0x%02x len=0x%x %s\n", tagtype, taglen, longtag ? "long" : "short");

		byte *buffer = new byte[taglen];
		if (fread(buffer, 1, taglen, fi) != taglen)
			DIE("can't read tag data\n");

		if (tagtype == 0x57) handle_DefineBinary(fi, fo, taglen, buffer);

		delete buffer;
	}

	fclose(fi);
	fclose(fo);

	return 0;
}
