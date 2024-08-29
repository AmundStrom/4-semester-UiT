#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMAGE_FILE "image"
#define ARGS "[--extended] [--vm] <bootblock> <executable-file> ..."

#define BOOTBLOCK_END 512
#define KERNEL_TEXT_END 4732
#define KERNEL_DATA_END 5120

/* Variable to store pointer to program name */
char *progname;

/* Variable to store pointer to the filename for the file being read. */
char *elfname;

/* Structure to store command line options */
static struct {
	int vm;
	int extended;
} options;

/* prototypes of local functions */
static void create_image(int nfiles, char *files[]);
static void error(char *fmt, ...);

void write_image(char *file, FILE *writef, char *files[]);
void magic_padding(FILE *writef, int size);
void padding(FILE *writef, int size);

int main(int argc, char **argv) {
	/* Process command line options */
	progname = argv[0];
	options.vm = 0;
	options.extended = 0;
	while ((argc > 1) && (argv[1][0] == '-') && (argv[1][1] == '-')) {
		char *option = &argv[1][2];

		if (strcmp(option, "vm") == 0) {
			options.vm = 1;
		}
		else if (strcmp(option, "extended") == 0) {
			options.extended = 1;
		}
		else {
			error("%s: invalid option\nusage: %s %s\n", progname, progname, ARGS);
		}
		argc--;
		argv++;
	}
	if (options.vm == 1) {
		/* This option is not needed in project 1 so we doesn't bother
		 * implementing it*/
		error("%s: option --vm not implemented\n", progname);
	}
	if (argc < 3) {
		/* at least 3 args (createimage bootblock kernel) */
		error("usage: %s %s\n", progname, ARGS);
	}
	create_image(argc - 1, argv + 1);
	return 0;
}

static void create_image(int nfiles, char *files[])
{	
	// File to be written to
	FILE *writef = fopen(IMAGE_FILE, "wb");

	for(int i = 0; i < nfiles; i++)
	{
		printf("Writing %s to image\n", files[i]);
		write_image(files[i], writef, files);
	}

	fclose(writef);
}


/**
 * @brief Will read and write the .text and .data section, if present, of the given file. Will also add necessary padding.
 * @param file File to be read, function will manage file
 * @param writef File to be written to, function will NOT manage file
 * @param files List of files that is being read
 */
void write_image(char *file, FILE *writef, char *files[])
{
	FILE *readf = fopen(file, "rb"); 	// File to be read
	Elf32_Ehdr elfhdr;					// ELF header struct
	int ret;
	int i;

	// Check if file is valid
	if(!readf){	
		error("Invalid file\n");
	}

	// Read ELF Header
	fseek(readf, 0, SEEK_SET);
	ret = fread(&elfhdr, sizeof(Elf32_Ehdr), 1, readf);
	if(ret != 1){
		error("Could not read ELF header\n");
	}

	// Check if file is an ELF file
	if(memcmp(elfhdr.e_ident, ELFMAG, SELFMAG) != 0){
		error("Not an ELF file\n");
	}

	// Create section header array equal to amount of sections
	Elf32_Shdr shdr[elfhdr.e_shnum];

	// Read sections and fill array
	for(i = 0; i < elfhdr.e_shnum; i++)
	{
		fseek(readf, elfhdr.e_shoff + (elfhdr.e_shentsize * i), SEEK_SET);
		ret = fread(&shdr[i], elfhdr.e_shentsize, 1, readf);
		if(ret != 1){
			error("Could not read Section header number: %i\n", i);
		}
	}

	char shstrtab[shdr[elfhdr.e_shnum - 1].sh_size];					// Create char with size of "Section Header String Table" (the last section)
	fseek(readf, shdr[elfhdr.e_shnum - 1].sh_offset, SEEK_SET);			// Seek at the last section in section header
	ret = fread(&shstrtab, shdr[elfhdr.e_shnum - 1].sh_size, 1, readf);	// Read at the last section in section header
	if(ret != 1){
		error("Could not read Section Header String Table\n");
	}

	// iterate over number of sections
	for(i = 0; i < elfhdr.e_shnum; i++)
	{	
		// Check if we encounter the ".text" and ".data" section
		if( strcmp(shdr[i].sh_name + shstrtab, ".text") == 0 || strcmp(shdr[i].sh_name + shstrtab, ".data") == 0 )
		{
			printf("writing: %s\n", shdr[i].sh_name + shstrtab);
			unsigned char buffer[shdr[i].sh_size];

			// Read section
			fseek(readf, shdr[i].sh_offset, SEEK_SET);
			ret = fread(&buffer, shdr[i].sh_size, 1, readf);
			if(ret != 1){
				printf("ret: %i\n", ret);
				error("Could not read %s Segment\n", shdr[i].sh_name + shstrtab);
			}

			// Write section to image
			fwrite(buffer, shdr[i].sh_size, 1, writef);

			// Only used when bootblock is written to image
			if( strcmp(file, files[0]) == 0 ){
				// Calculate size of padding, and write padding and magic number to image
				int size = sizeof(buffer);
				magic_padding(writef, (BOOTBLOCK_END - size));
				
				// Write kernel sectors to the image
				fseek(writef, 2, SEEK_SET);
				fwrite(&elfhdr.e_shstrndx, sizeof(unsigned short), 1, writef);
				fseek(writef, 0, SEEK_END);
			}

			// Only used when kernel .text section is written to image
			if( strcmp(file, files[1]) == 0 && strcmp(shdr[i].sh_name + shstrtab, ".text") == 0 ){
				// Calculate size of padding and write padding
				int size = sizeof(buffer);
				padding(writef, (KERNEL_TEXT_END - BOOTBLOCK_END - size));
			}

			// Only used when kernel .data section is written to image
			if( strcmp(file, files[1]) == 0 && strcmp(shdr[i].sh_name + shstrtab, ".data") == 0 ){
				// Calculate size of padding and write padding
				int size = sizeof(buffer);
				padding(writef, (KERNEL_DATA_END - KERNEL_TEXT_END - size));
			}
		}
	}

	fclose(readf);
	return;
}

/* Will add padding with the given size in bytes and magic number into a file */
void magic_padding(FILE *writef, int size)
{
	unsigned char buffer[size];
	memset(buffer, 0, sizeof(buffer) );

	buffer[sizeof(buffer) - 2] = 0x55;
	buffer[sizeof(buffer) - 1] = 0xaa;

	fwrite(buffer, sizeof(buffer), 1, writef);
}

/* Will add padding with the given size in bytes into a file */
void padding(FILE *writef, int size)
{
	unsigned char buffer[size];
	memset(buffer, 0, sizeof(buffer) );
	fwrite(buffer, sizeof(buffer), 1, writef);
}

/* print an error message and exit */
static void error(char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	if (errno != 0) {
		perror(NULL);
	}
	exit(EXIT_FAILURE);
}
