
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <windows.h>
#include "rapidxml/rapidxml.hpp"
#include "ugon.h"
#include "gon/gon.h"

__int64 CounterStart = 0;

typedef union time_64 {
	uint64_t t64;
	struct {
		uint32_t t32_low;
		uint32_t t32_high;
	};
} time_64;
 
#define start_timer(cycles_low, cycles_high) asm volatile (\
	"CPUID\n\t"/*serialize*/\
	"RDTSC\n\t"/*read the clock*/\
	"mov %%edx, %0\n\t"\
	"mov %%eax, %1\n\t": "=r" (cycles_high), "=r"\
	(cycles_low):: "%rax", "%rbx", "%rcx", "%rdx");\

#define stop_timer(cycles_low, cycles_high) __asm __volatile (\
	"RDTSCP\n\t"/*read the clock*/\
	"mov %%edx, %0\n\t"\
	"mov %%eax, %1\n\t"\
	"CPUID\n\t": "=r" (cycles_high), "=r"\
	(cycles_low):: "%rax", "%rbx", "%rcx", "%rdx");\


inline void start_perf_counter(void) {
	LARGE_INTEGER li;
	if (!QueryPerformanceFrequency(&li))
		puts("QueryPerformanceFrequency failed!\n");

	QueryPerformanceCounter(&li);
	CounterStart = li.QuadPart;
}

inline double get_perf_counter(void) {
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return (double)(li.QuadPart - CounterStart)/* / PCFreq*/;
}

void read_text_file(const char* file_name, char** buffer, size_t* size) {
	FILE* fp;

	fopen_s(&fp, file_name, "rb");
	if (!fp) perror(file_name), exit(1);

	fseek(fp, 0L, SEEK_END);
	*size = ftell(fp);
	rewind(fp);

	*buffer = (char*)malloc(sizeof(char) * (*size + 1L));
	if (!*buffer) fclose(fp), fputs("memory alloc fails", stderr), exit(1);

	if (!fread(*buffer, sizeof(char), *size, fp))
		fclose(fp), free(*buffer), fputs("entire read fails", stderr), exit(1);

	fclose(fp);
}

void gon_test(void) {
	GonFile gon = gon_create();
	read_text_file("test.gon", &gon.file, &gon.file_length);
	gon_parse(&gon);

	char buf[1024];
	gon_serialize(&gon, buf, 2);

	puts(buf);

	gon_free(&gon);
}

uint64_t time_strlen(const char* buffer, size_t size, uint64_t reps) {
	int len = 0;
	time_64 start_time, end_time, 
		min_time = {.t64 = INT64_MAX}, 
		sum_time = {.t64 = 0};

	for (int i = 0; i < reps; i++) {
		start_timer(start_time.t32_low, start_time.t32_high);
		len = strlen(buffer);
		stop_timer(end_time.t32_low, end_time.t32_high);

		time_64 this_time;
		this_time.t64 = end_time.t64 - start_time.t64;
		sum_time.t64 += this_time.t64;
		if (this_time.t64 < min_time.t64) min_time.t64 = this_time.t64;
	}

	printf("strlen timing report:\nsum time: %llu\navg time: %llu\nmin time: %llu\n", sum_time.t64, sum_time.t64 / reps, min_time.t64);
	printf("len: %i\n\n", len);

	return sum_time.t64;
}

uint64_t time_ugon(const char* buffer, size_t size, uint64_t reps) {
	GonFile gon = gon_create();
	char* buffer_copy = (char*)malloc(sizeof(char) * (size + 1L));

	gon.file = buffer_copy;
	gon.file_length = size;

	time_64 start_time, end_time, 
		min_time = {.t64 = INT64_MAX}, 
		sum_time = {.t64 = 0};

	for (int i = 0; i < reps; i++) {
		memcpy(buffer_copy, buffer, sizeof(char) * (size + 1L));

		start_timer(start_time.t32_low, start_time.t32_high);
		gon_parse(&gon);
		stop_timer(end_time.t32_low, end_time.t32_high);

		time_64 this_time;
		this_time.t64 = end_time.t64 - start_time.t64;
		sum_time.t64 += this_time.t64;
		if (this_time.t64 < min_time.t64) min_time.t64 = this_time.t64;
	}

	printf("ugon timing report:\nsum time: %llu\navg time: %llu\nmin time: %llu\n\n", sum_time.t64, sum_time.t64 / reps, min_time.t64);

	gon_free(&gon);

	return sum_time.t64;
}

uint64_t time_gon(const char* buffer, uint64_t reps) {
	time_64 start_time, end_time, 
		min_time = {.t64 = INT64_MAX}, 
		sum_time = {.t64 = 0};

	std::string str(buffer);

	for (int i = 0; i < reps; i++) {
		std::string string_copy = str;
		start_timer(start_time.t32_low, start_time.t32_high);
		GonObject gon = GonObject::LoadFromBuffer(string_copy);
		stop_timer(end_time.t32_low, end_time.t32_high);

		time_64 this_time;
		this_time.t64 = end_time.t64 - start_time.t64;
		sum_time.t64 += this_time.t64;
		if (this_time.t64 < min_time.t64) min_time.t64 = this_time.t64;
	}

	printf("gon timing report:\nsum time: %llu\navg time: %llu\nmin time: %llu\n\n", sum_time.t64, sum_time.t64 / reps, min_time.t64);

	return sum_time.t64;
}

uint64_t time_rapidxml(char* buffer, size_t size, uint64_t reps) {
	using namespace rapidxml;

	char* buffer_copy = (char*)malloc(sizeof(char) * (size + 1L));;

	time_64 start_time, end_time, 
		min_time = {.t64 = INT64_MAX}, 
		sum_time = {.t64 = 0};

	for (int i = 0; i < reps; i++) {
		memcpy(buffer_copy, buffer, sizeof(char) * (size + 1L));
		xml_document<> doc;
		start_timer(start_time.t32_low, start_time.t32_high);
		doc.parse<0>(buffer_copy);
		stop_timer(end_time.t32_low, end_time.t32_high);

		time_64 this_time;
		this_time.t64 = end_time.t64 - start_time.t64;
		sum_time.t64 += this_time.t64;
		if (this_time.t64 < min_time.t64) min_time.t64 = this_time.t64;
	}

	free(buffer_copy);

	printf("rapidxml timing report:\nsum time: %llu\navg time: %llu\nmin time: %llu\n", sum_time.t64, sum_time.t64 / reps, min_time.t64);

	return sum_time.t64;
}


void speed_test(void) {
	printf("sizeof(GonField): %llu\n\n", sizeof(GonField));

	int reps = 10000000;
	int len = 0;

	char* buffer;
	size_t size;
	read_text_file("test.gon", &buffer, &size);
	buffer[size] = 0;
	int gon_len = size;

	uint64_t strlen_sum_time = time_strlen(buffer, size, reps);

	//uint64_t gon_sum_time = time_gon(buffer, reps);

	uint64_t ugon_sum_time = time_ugon(buffer, size, reps);
	free(buffer);

	read_text_file("test.xml", &buffer, &size);
	buffer[size] = 0;
	int xml_len = size;
	uint64_t rapidxml_sum_time = time_rapidxml(buffer, size, reps);
	free(buffer);

	puts("\nTiming ratios:");
	printf("ugon time     /  strlen time: %f\n", (double)ugon_sum_time     / (double)strlen_sum_time);
	//printf(" gon time     /  ugon time: %f\n",   (double)gon_sum_time      / (double)ugon_sum_time);
	printf("rapidxml time /  ugon time: %f\n",   (double)rapidxml_sum_time / (double)ugon_sum_time);
	
	puts("\nTime spent per character:");
	printf("strlen time / gon len: %f\n", ((double)strlen_sum_time   / (double)reps) / (double)gon_len);
	//printf("   gon time / gon len: %f\n", ((double)gon_sum_time      / (double)reps) / (double)gon_len);
	printf("  ugon time / gon len: %f\n", ((double)ugon_sum_time     / (double)reps) / (double)gon_len);
	printf("   xml time / xml len: %f\n", ((double)rapidxml_sum_time / (double)reps) / (double)xml_len);

	puts("\nFile Lengths:");
	printf("gon_len: %i\n", gon_len);
	printf("xml_len: %i\n", xml_len);
}

int main(void) {
	gon_test();
	speed_test();
	return 0;
}