/* Copyright (c) 2018-2018, David Anderson
All rights reserved.

Redistribution and use in source and binary forms, with
or without modification, are permitted provided that the
following conditions are met:

    Redistributions of source code must retain the above
    copyright notice, this list of conditions and the following
    disclaimer.

    Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following
    disclaimer in the documentation and/or other materials
    provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config.h"
#include <stdio.h>
#include <sys/types.h> /* fstat */
#include <sys/stat.h> /* fstat */
#include <fcntl.h> /* O_RDONLY */
#ifdef HAVE_UNISTD_H
#include <unistd.h> /* lseek read close */
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_STRING_H
#include <string.h> /* memcpy, strcpy */
#endif /* HAVE_STRING_H */
#include "libdwarf.h"
#include "dwarf_object_read_common.h"
#include "dwarf_object_detector.h"

/* This is the main() program for the object_detector executable. */

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif /* TRUE */

#ifndef O_RDONLY
#define O_RDONLY 0
#endif


#define DW_DLV_NO_ENTRY -1
#define DW_DLV_OK        0
#define DW_DLV_ERROR     1

#ifndef EI_NIDENT
#define EI_NIDENT 16
#define EI_CLASS  4
#define EI_DATA   5
#define EI_VERSION 6
#define ELFCLASS32 1
#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define ELFDATA2MSB 2
#endif /* EI_NIDENT */

#define DSYM_SUFFIX ".dSYM/Contents/Resources/DWARF/"
#define PATHSIZE 2000

/*  Assuming short 16 bits, unsigned 32 bits */

typedef unsigned DW_TYPEOF_16BIT t16;
typedef unsigned DW_TYPEOF_32BIT t32;

#ifndef  MH_MAGIC
/* mach-o 32bit */
#define MH_MAGIC        0xfeedface
#define MH_CIGAM        0xcefaedfe
#endif /*  MH_MAGIC */
#ifndef  MH_MAGIC_64
/* mach-o 64bit */
#define MH_MAGIC_64 0xfeedfacf
#define MH_CIGAM_64 0xcffaedfe
#endif /*  MH_MAGIC_64 */

#ifdef WORDS_BIGENDIAN
#define ASSIGN(func,t,s)                        \
    do {                                        \
        unsigned tbyte = sizeof(t) - sizeof(s); \
        t = 0;                                  \
        func(((char *)t)+tbyte ,&s,sizeof(s));  \
    } while (0)
#else /* LITTLE ENDIAN */
#define ASSIGN(func,t,s)                        \
    do {                                        \
        t = 0;                                  \
        func(&t,&s,sizeof(s));                  \
    } while (0)
#endif /* end LITTLE- BIG-ENDIAN */


#define EI_NIDENT 16
/* An incomplete elf header, good for 32 and 64bit elf */
struct elf_header {
    unsigned char  e_ident[EI_NIDENT];
    t16 e_type;
    t16 e_machine;
    t32 e_version;
};

/*  Windows. Certain PE objects.
    The following references may be of interest.
https://msdn.microsoft.com/library/windows/desktop/ms680547(v=vs.85).aspx       #PE format overview and various machine magic numbers

https://msdn.microsoft.com/en-us/library/ms809762.aspx  # describes some details of PE headers, basically an overview

https://msdn.microsoft.com/en-us/library/windows/desktop/aa383751(v=vs.85).aspx #defines sizes of various types

https://msdn.microsoft.com/fr-fr/library/windows/desktop/ms680313(v=vs.85).aspx #defines IMAGE_FILE_HEADER and Machine fields (32/64)

https://msdn.microsoft.com/fr-fr/library/windows/desktop/ms680305(v=vs.85).aspx #defines IMAGE_DATA_DIRECTORY

https://msdn.microsoft.com/en-us/library/windows/desktop/ms680339(v=vs.85).aspx #Defines IMAGE_OPTIONAL_HEADER and some magic numbers

https://msdn.microsoft.com/fr-fr/library/windows/desktop/ms680336(v=vs.85).aspx # defines _IMAGE_NT_HEADERS 32 64

https://msdn.microsoft.com/en-us/library/windows/desktop/ms680341(v=vs.85).aspx # defines _IMAGE_SECTION_HEADER

*/

/* ===== START pe structures */

struct dos_header {
    t16  dh_mz;
    char dh_dos_data[58];
    t32  dh_image_offset;
};

#define IMAGE_DOS_SIGNATURE      0x5A4D
#define IMAGE_DOS_REVSIGNATURE   0x4D5A
#define IMAGE_NT_SIGNATURE       0x00004550
#define IMAGE_FILE_MACHINE_I386  0x14c
#define IMAGE_FILE_MACHINE_IA64  0x200
#define IMAGE_FILE_MACHINE_AMD64 0x8886


struct pe_image_file_header {
    t16 im_machine;
    t16 im_sectioncount;
    t32 im_ignoring[3];
    t16 im_opt_header_size;
    t16 im_ignoringb;
};

/* ===== END pe structures */

static void *
memcpy_swap_bytes(void *s1, const void *s2, size_t len)
{
    void *orig_s1 = s1;
    unsigned char *targ = (unsigned char *) s1;
    const unsigned char *src = (const unsigned char *) s2;

    if (len == 4) {
        targ[3] = src[0];
        targ[2] = src[1];
        targ[1] = src[2];
        targ[0] = src[3];
    } else if (len == 8) {
        targ[7] = src[0];
        targ[6] = src[1];
        targ[5] = src[2];
        targ[4] = src[3];
        targ[3] = src[4];
        targ[2] = src[5];
        targ[1] = src[6];
        targ[0] = src[7];
    } else if (len == 2) {
        targ[1] = src[0];
        targ[0] = src[1];
    }
/* should NOT get below here: is not the intended use */
    else if (len == 1) {
        targ[0] = src[0];
    } else {
        memcpy(s1, s2, len);
    }
    return orig_s1;
}


/*  For following MacOS file naming convention */
static const char *
getseparator (const char *f)
{
    const char *p = 0;
    const char *q = 0;
    char c = 0;;

    p = NULL;
    q = f;
    do  {
        c = *q++;
        if (c == '\\' || c == '/' || c == ':') {
            p = q;
        }
    } while (c);
    return p;
}

static const char *
getbasename (const char *f)
{
    const char *pseparator = getseparator (f);
    if (!pseparator) {
        return f;
    }
    return pseparator;
}

/*  Not a standard function, though part of GNU libc
    since 2008 (I have never examined the GNU version).  */
static char *
dw_stpcpy(char *dest,const char *src)
{
    const char *cp = src;
    char *dp = dest;

    for ( ; *cp; ++cp,++dp) {
        *dp = *cp;
    }
    *dp = 0;
    return dp;
}



/* This started like Elf, so check initial fields. */
static int
fill_in_elf_fields(struct elf_header *h,
    unsigned *endian,
    /*  Size of the object file offsets, not DWARF offset
        size. */
    unsigned *objoffsetsize,
    int *errcode)
{
    unsigned locendian = 0;
    unsigned locoffsetsize = 0;
#if 0
    void *(*word_swap) (void *, const void *, size_t);
#endif

    switch(h->e_ident[EI_CLASS]) {
    case ELFCLASS32:
        locoffsetsize = 32;
        break;
    case ELFCLASS64:
        locoffsetsize = 64;
        break;
    default:
        *errcode = DW_DLE_ELF_CLASS_BAD;
        return DW_DLV_ERROR;
    }
    switch(h->e_ident[EI_DATA]) {
    case ELFDATA2LSB:
        locendian = DW_ENDIAN_LITTLE;
#if 0
#ifdef WORDS_BIGENDIAN
        word_swap = memcpy_swap_bytes;
#else  /* LITTLE ENDIAN */
        word_swap = memcpy;
#endif /* LITTLE- BIG-ENDIAN */
#endif
        break;
    case ELFDATA2MSB:
        locendian = DW_ENDIAN_BIG;
#if 0
#ifdef WORDS_BIGENDIAN
        word_swap = memcpy;
#else  /* LITTLE ENDIAN */
        word_swap = memcpy_swap_bytes;
#endif /* LITTLE- BIG-ENDIAN */
#endif
        break;
    default:
        *errcode = DW_DLE_ELF_ENDIAN_BAD;
        return DW_DLV_ERROR;
    }
    if (h->e_ident[EI_VERSION] != 1 /* EV_CURRENT */) {
        *errcode = DW_DLE_ELF_VERSION_BAD;
        return DW_DLV_ERROR;
    }
    *endian = locendian;
    *objoffsetsize = locoffsetsize;
    return DW_DLV_OK;
}
static char archive_magic[8] = {
'!','<','a','r','c','h','>',0x0a
};
static int
is_archive_magic(struct elf_header *h) {
    int i = 0;
    int len = sizeof(archive_magic);
    const char *cp = (const char *)h;
    for( ; i < len; ++i) {
        if (cp[i] != archive_magic[i]) {
            return FALSE;
        }
    }
    return TRUE;
}

/*  A bit unusual in that it always sets *is_pe_flag
    Return of DW_DLV_OK  it is a PE file we recognize. */
static int
is_pe_object(int fd,
    unsigned long filesize,
    unsigned *endian,
    unsigned *offsetsize,
    int *errcode)
{
    t16 dos_sig;
    unsigned locendian = 0;
    void *(*word_swap) (void *, const void *, size_t);
    t32 nt_address = 0;
    struct dos_header dhinmem;
    t16 nt_sig = 0;
    struct pe_image_file_header ifh;
    int res = 0;

    if (filesize < (sizeof (struct dos_header) +
        sizeof(t32) + sizeof(struct pe_image_file_header))) {
        *errcode = DW_DLE_FILE_TOO_SMALL;
        return DW_DLV_ERROR;
    }
    res = _dwarf_object_read_random(fd,(char *)&dhinmem,
        0,sizeof(dhinmem),errcode);
    if (res != DW_DLV_OK) {
        return res;
    }
    dos_sig = dhinmem.dh_mz;
    if (dos_sig == IMAGE_DOS_SIGNATURE) {
#ifdef WORDS_BIGENDIAN
        word_swap = memcpy_swap_bytes;
#else  /* LITTLE ENDIAN */
        word_swap = memcpy;
#endif /* LITTLE- BIG-ENDIAN */
        locendian = DW_ENDIAN_LITTLE;
    } else if (dos_sig == IMAGE_DOS_REVSIGNATURE) {
        locendian = DW_ENDIAN_BIG;
#ifdef WORDS_BIGENDIAN
        word_swap = memcpy;
#else  /* LITTLE ENDIAN */
        word_swap = memcpy_swap_bytes;
#endif /* LITTLE- BIG-ENDIAN */
    } else {
        /* Not dos header not a PE file we recognize */
        *errcode = DW_DLE_FILE_WRONG_TYPE;
        return DW_DLV_ERROR;
    }
    ASSIGN(word_swap,nt_address, dhinmem.dh_image_offset);
    if (filesize < nt_address) {
        /* Not dos header not a PE file we recognize */
        *errcode = DW_DLE_FILE_TOO_SMALL;
        return DW_DLV_ERROR;
    }
    if (filesize < (nt_address + sizeof(t32) +
        sizeof(struct pe_image_file_header))) {
        *errcode = DW_DLE_FILE_TOO_SMALL;
        /* Not dos header not a PE file we recognize */
        return DW_DLV_ERROR;
    }
    res =  _dwarf_object_read_random(fd,(char *)&nt_sig,nt_address,
        sizeof(nt_sig),errcode);
    if (res != DW_DLV_OK) {
        return res;
    }
    {   t32 lsig = 0;
        ASSIGN(word_swap,lsig,nt_sig);
        nt_sig = lsig;
    }
    if (nt_sig != IMAGE_NT_SIGNATURE) {
        *errcode = DW_DLE_FILE_WRONG_TYPE;
        return DW_DLV_ERROR;
    }
    res = _dwarf_object_read_random(fd,(char *)&ifh,
        nt_address + sizeof(t32),
        sizeof(struct pe_image_file_header),
        errcode);
    if (res != DW_DLV_OK) {
        return res;
    }
    {
        t32 machine = 0;

        ASSIGN(word_swap,machine,ifh.im_machine);
        switch(machine) {
        case IMAGE_FILE_MACHINE_I386:
            *offsetsize = 32;
            *endian = locendian;
            return DW_DLV_OK;
        case IMAGE_FILE_MACHINE_IA64:
        case IMAGE_FILE_MACHINE_AMD64:
            *offsetsize = 64;
            *endian = locendian;
            return DW_DLV_OK;
        }
    }
    /*  There are lots more machines,
        we are unsure which are of interest. */
    *errcode = DW_DLE_FILE_WRONG_TYPE;
    return DW_DLV_ERROR;
}

static int
is_mach_o_magic(struct elf_header *h,
    unsigned *endian,
    unsigned *offsetsize)
{
    t32 magicval = 0;
    unsigned locendian = 0;
    unsigned locoffsetsize = 0;

    memcpy(&magicval,h,sizeof(magicval));
    if (magicval == MH_MAGIC) {
        locendian = DW_ENDIAN_SAME;
        locoffsetsize = 32;
    } else if (magicval == MH_CIGAM) {
        locendian = DW_ENDIAN_OPPOSITE;
        locoffsetsize = 32;
    }else if (magicval == MH_MAGIC_64) {
        locendian = DW_ENDIAN_SAME;
        locoffsetsize = 64;
    } else if (magicval == MH_CIGAM_64) {
        locendian = DW_ENDIAN_OPPOSITE;
        locoffsetsize = 64;
    } else {
        return FALSE;
    }
    *endian = locendian;
    *offsetsize = locoffsetsize;
    return TRUE;
}

int
dwarf_object_detector_fd(int fd,
    unsigned *ftype,
    unsigned *endian,
    unsigned *offsetsize,
    Dwarf_Unsigned  *filesize,
    int *errcode)
{
    struct elf_header h;
    size_t readlen = sizeof(h);
    int res = 0;
    off_t fsize = 0;
    off_t lsval = 0;
    ssize_t readval = 0;

    if (sizeof(t32) != 4 || sizeof(t16)!= 2) {
        *errcode = DW_DLE_BAD_TYPE_SIZE;
        return DW_DLV_ERROR;
    }
    fsize = lseek(fd,0L,SEEK_END);
    if(fsize < 0) {
        *errcode = DW_DLE_SEEK_ERROR;
        return DW_DLV_ERROR;
    }
    if (fsize <= (off_t)readlen) {
        /* Not a real object file */
        *errcode = DW_DLE_FILE_TOO_SMALL;
        return DW_DLV_ERROR;
    }
    lsval  = lseek(fd,0L,SEEK_SET);
    if(lsval < 0) {
        *errcode = DW_DLE_SEEK_ERROR;
        return DW_DLV_ERROR;
    }
    readval = read(fd,&h,readlen);
    if (readval != (ssize_t)readlen) {
        *errcode = DW_DLE_READ_ERROR;
        return DW_DLV_ERROR;
    }
    if (h.e_ident[0] == 0x7f &&
        h.e_ident[1] == 'E' &&
        h.e_ident[2] == 'L' &&
        h.e_ident[3] == 'F') {
        /* is ELF */

        res = fill_in_elf_fields(&h,endian,offsetsize,errcode);
        if (res != DW_DLV_OK) {
            return res;
        }
        *ftype = DW_FTYPE_ELF;
        *filesize = (size_t)fsize;
        return DW_DLV_OK;
    }
    if (is_mach_o_magic(&h,endian,offsetsize)) {
        *ftype = DW_FTYPE_MACH_O;
        *filesize = (size_t)fsize;
        return DW_DLV_OK;
    }
    if (is_archive_magic(&h)) {
        *ftype = DW_FTYPE_ARCHIVE;
        *filesize = (size_t)fsize;
        return DW_DLV_OK;
    }
    res = is_pe_object(fd,fsize,endian,offsetsize,errcode);
    if (res == DW_DLV_OK ) {
        *ftype = DW_FTYPE_PE;
        *filesize = (size_t)fsize;
        return DW_DLV_OK;
    }
    /* CHECK FOR  PE object. */
    return DW_DLV_NO_ENTRY;
}

int
dwarf_object_detector_path(const char  *path,
    char *outpath,size_t outpath_len,
    unsigned *ftype,
    unsigned *endian,
    unsigned *offsetsize,
    Dwarf_Unsigned  *filesize,
    int *errcode)
{
    char *cp = 0;
    size_t plen = strlen(path);
    size_t dsprefixlen = sizeof(DSYM_SUFFIX);
    struct stat statbuf;
    int fd = -1;
    int res = 0;

#if !defined(S_ISREG)
#define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
#endif
#if !defined(S_ISDIR)
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif

    res = stat(path,&statbuf);
    if(res) {
        return DW_DLV_NO_ENTRY;
    }
    if ((2*plen + dsprefixlen +2) >= outpath_len) {
        *errcode =  DW_DLE_PATH_SIZE_TOO_SMALL;
        return DW_DLV_ERROR;
    }
    cp = dw_stpcpy(outpath,path);
    cp = dw_stpcpy(cp,DSYM_SUFFIX);
    dw_stpcpy(cp,getbasename(path));

    fd = open(outpath,O_RDONLY);
    if (fd < 0) {
        *outpath = 0;
        fd = open(path,O_RDONLY);
        dw_stpcpy(outpath,path);
    }
    if (fd < 0) {
        *outpath = 0;
        return DW_DLV_NO_ENTRY;
    }
    res = dwarf_object_detector_fd(fd,
        ftype,endian,offsetsize,filesize,errcode);
    if (res != DW_DLV_OK) {
        *outpath = 0;
    }
    close(fd);
    return res;
}