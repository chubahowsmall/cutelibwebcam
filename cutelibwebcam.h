/*
	CuteLibWebcam is an easy to use and learn abstraction for v4l2 interface
			Copyright (C) 2010  Joaquín Bogado

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef __CUTELIBWEBCAM_H
#define __CUTELIBWEBCAM_H

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

/*Error numbers returned by functions and processed by cam_catcherror*/
#define ERR_NOVIDEODEV 1
#define ERR_MALLOC 2
#define ERR_OPENDEV 3
#define ERR_CLOSEDEV 4
#define ERR_QUERYCAP 5
#define ERR_IOMETHOD 6
#define ERR_VIDIOCSFMT 7
#define ERR_VIDIOCGFMT 8
#define ERR_VIDIOREQBUFS 9
#define ERR_NOENOUGHMEM 10
#define ERR_VIDIOQUERYBUF 11
#define ERR_MMAP 12
#define ERR_VIDIOQBUF 13
#define ERR_VIDIOSTREAMON 14


#define IO_METHOD_READ 0
#define IO_METHOD_MMAP 1
#define IO_METHOD_USERPTR 2

/*Web Cam device types and operations*/
typedef	struct {
	void   *start;
	size_t  length;
}buffer_t;
typedef struct {
	int fd;
	char * name;
	int iomethod;
	buffer_t * buf;
	int n_buf;
	struct v4l2_capability * cap;
	struct v4l2_cropcap * cropcap;
	struct v4l2_crop * crop;
    struct v4l2_format * fmt;
} camdevice;



/*All functions returns 0 when no error or ERR_* */
int cam_catcherror(int err);
int cam_open(camdevice * cam);
int cam_init(camdevice *cam); 
int cam_close(camdevice * cam);
int cam_setdevname(camdevice * cam, char * name);
int cam_setiomethod(camdevice * cam, int iomethod);/*iomethod must be one of IO_METHOD_READ, IO_METHOD_MMAP, IO_METHOD_USERPTR,*/
int cam_setbuffer(camdevice * cam, buffer_t * b);
int cam_startcapturing(camdevice * cam);
int cam_stopcapturing(camdevice * cam);
int cam_uninit(camdevice * cam);

#endif
