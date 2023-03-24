
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <windows.h>
#include "rapidxml/rapidxml.hpp"
#include "ugon.h"
#include "gon/gon.h"

__int64 CounterStart = 0;

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
	gon_serialize(&gon, buf);

	puts(buf);

	gon_free(&gon);

	printf("sizeof(GonField): %u\n\n", sizeof(GonField));
}

double time_strlen(const char* buffer, size_t size, int reps) {
	int len = 0;
	double	sum_time = 0,
			min_time = 999999;

	for (int i = 0; i < reps; i++) {
		start_perf_counter();
		len = strlen(buffer);
		double this_time = get_perf_counter();
		sum_time += this_time;
		if (this_time < min_time) min_time = this_time;
	}

	printf("strlen timing report:\nsum time: %f\navg time: %f\nmin time: %f\n", sum_time, sum_time / reps, min_time);
	printf("len: %i\n\n", len);

	return sum_time;
}

double time_ugon(const char* buffer, size_t size, int reps) {
	GonFile gon = gon_create();
	char* buffer_copy = (char*)malloc(sizeof(char) * (size + 1L));

	gon.file = buffer_copy;
	gon.file_length = size;

	double sum_time = 0,
		   min_time = 999999;

	for (int i = 0; i < reps; i++) {
		memcpy(buffer_copy, buffer, sizeof(char) * (size + 1L));
		start_perf_counter();
		gon_parse(&gon);
		double this_time = get_perf_counter();
		sum_time += this_time;
		if (this_time < min_time) min_time = this_time;
	}

	printf("ugon timing report:\nsum time: %f\navg time: %f\nmin time: %f\n\n", sum_time, sum_time / reps, min_time);

	gon_free(&gon);

	return sum_time;
}

double time_gon(const std::string& filename, int reps) {
	double sum_time = 0,
		   min_time = 999999;

	for (int i = 0; i < reps; i++) {
		start_perf_counter();
		GonObject gon = GonObject::Load(filename);
		double this_time = get_perf_counter();
		sum_time += this_time;
		if (this_time < min_time) min_time = this_time;
	}

	printf("gon timing report:\nsum time: %f\navg time: %f\nmin time: %f\n\n", sum_time, sum_time / reps, min_time);

	return sum_time;
}

double time_rapidxml(char* buffer, size_t size, int reps) {
	using namespace rapidxml;

	char* buffer_copy = (char*)malloc(sizeof(char) * (size + 1L));;

	double sum_time = 0,
		   min_time = 999999;


	for (int i = 0; i < reps; i++) {
		memcpy(buffer_copy, buffer, sizeof(char) * (size + 1L));
		xml_document<> doc;
		start_perf_counter();
		doc.parse<0>(buffer_copy);
		double this_time = get_perf_counter();
		sum_time += this_time;
		if (this_time < min_time) min_time = this_time;
	}

	free(buffer_copy);

	printf("rapidxml timing report:\nsum time: %f\navg time: %f\nmin time: %f\n\n", sum_time, sum_time / reps, min_time);

	return sum_time;
}


void speed_test(void) {
	int reps = 1000000;
	int len = 0;

	char* buffer;
	size_t size;
	read_text_file("test.gon", &buffer, &size);
	buffer[size] = 0;
	int gon_len = strlen(buffer);

	double strlen_sum_time = time_strlen(buffer, size, reps);

	double gon_sum_time = time_gon("test.gon", reps);

	double ugon_sum_time = time_ugon(buffer, size, reps);
	free(buffer);

	read_text_file("test.xml", &buffer, &size);
	buffer[size] = 0;
	double rapidxml_sum_time = time_rapidxml(buffer, size, reps);
	int xml_len = strlen(buffer);
	free(buffer);

	printf("ugon time / strlen time: %f\n", ugon_sum_time / strlen_sum_time);
	printf("ugon time / rapidxml time: %f\n", ugon_sum_time / rapidxml_sum_time);
	printf("ugon time / gon time: %f\n\n", ugon_sum_time / gon_sum_time);

	printf("ugon time / gon len: %f\n", ugon_sum_time / (double)(gon_len * reps));
	printf("xml time / xml len: %f\n", rapidxml_sum_time / (double)(xml_len * reps));
	printf("gon time / gon len: %f\n\n", gon_sum_time / (double)(gon_len * reps));

	printf("gon_len: %i\n", gon_len);
	printf("xml_len: %i\n", xml_len);
}

int main(void) {

	gon_test();
	speed_test();

	return 0;
}