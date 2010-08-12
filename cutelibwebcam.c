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

#include "cutelibwebcam.h"

int cam_open(camdevice * cam){
	cam->fd = open(cam->name, O_RDWR|O_NONBLOCK, 0);
	if (cam->fd == -1){
		return ERR_OPENDEV;
	}
	return 0;
}

int cam_close(camdevice * cam){
	if (close(cam->fd))
		return ERR_CLOSEDEV; //Never happend?
	cam->fd = -1;
	return 0;
}

/********* private, don't use this functions ******************/
int _xioctl(int fh, int request, void *arg){
	int r;

	do {
			r = ioctl(fh, request, arg);
	} while (-1 == r && EINTR == errno);

	return r;
}

int init_read(camdevice * cam, unsigned int buffer_size){
	buffer_t * b;
	b = calloc(1, sizeof(buffer_t));
	if (!b) {
		return ERR_MALLOC;
	}
	b[0].length = buffer_size;
	b[0].start = malloc(buffer_size);
	if (!b[0].start) {
		return ERR_MALLOC;
	}
	cam_setbuffer(cam, b);
	return 0;
}
int init_mmap(camdevice * cam, unsigned int buffer_size){
	unsigned int n_buffers;
	buffer_t * b;
	struct v4l2_requestbuffers req;
	memset(&req, 0, sizeof(req));
	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;
	if (-1 == _xioctl(cam->fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			return ERR_IOMETHOD;
		} else {
			return ERR_VIDIOREQBUFS;
		}
	}
	if (req.count < 2) {
		return ERR_NOENOUGHMEM;
	}
	b = calloc(req.count, sizeof(buffer_t));
	if (!b) {
		return ERR_MALLOC;
	}
	for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
		struct v4l2_buffer buf;
		memset(&buf, 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = n_buffers;

		if (-1 == _xioctl(cam->fd, VIDIOC_QUERYBUF, &buf))
			return ERR_VIDIOQUERYBUF;

		b[n_buffers].length = buf.length;
		b[n_buffers].start = mmap(NULL /* start anywhere */, buf.length, PROT_READ | PROT_WRITE /* required */,MAP_SHARED /* recommended */,
						cam->fd, buf.m.offset);

		if (MAP_FAILED == b[n_buffers].start)
			return ERR_MMAP;
	}
	cam_setbuffer(cam, b);
	cam->n_buf = n_buffers;
	return 0;
}
int init_usrp(camdevice * cam, unsigned int buffer_size){
	buffer_t * b;
	unsigned int n_buffers;
	struct v4l2_requestbuffers req;
	memset(&req, 0, sizeof(req));
	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_USERPTR;
	if (-1 == _xioctl(cam->fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			return ERR_IOMETHOD;
		}
		else {
			return ERR_VIDIOREQBUFS;
		}
	}
	b = calloc(4, sizeof(buffer_t));
	if (!b) {
		return ERR_MALLOC;
	}
	for (n_buffers = 0; n_buffers < 4; ++n_buffers) {
		b[n_buffers].length = buffer_size;
		b[n_buffers].start = malloc(buffer_size);
		if (!b[n_buffers].start) {
			return ERR_MALLOC;
		}
	}
	cam_setbuffer(cam, b);
	cam->n_buf = n_buffers;
	return 0;
}
int (*__init[])(camdevice * cam, unsigned int buffer_size) = {init_read, init_mmap, init_usrp};
/********* End of private section *******************/

int cam_init(camdevice * cam){

	int force_format = 0;
	int min;
	cam->cap = calloc(sizeof(struct v4l2_capability), 1);
	cam->cropcap = calloc(sizeof(struct v4l2_cropcap), 1);
	cam->crop = calloc(sizeof(struct v4l2_crop), 1);
	cam->fmt = calloc(sizeof(struct v4l2_format), 1);
	
	if ((cam->cap == NULL)||(cam->cropcap == NULL)||(cam->cropcap == NULL)||(cam->fmt == NULL)){
		return ERR_MALLOC;
	}
	
	if (-1 == _xioctl(cam->fd, VIDIOC_QUERYCAP, cam->cap)) {
		return ERR_QUERYCAP;
	}

	if (!(cam->cap->capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		return ERR_NOVIDEODEV;
	}

	switch (cam->iomethod) {
	case IO_METHOD_READ:
		if (!(cam->cap->capabilities & V4L2_CAP_READWRITE)) {
			return ERR_IOMETHOD;
		}
		break;
	case IO_METHOD_MMAP:
	case IO_METHOD_USERPTR:
		if (!(cam->cap->capabilities & V4L2_CAP_STREAMING)) {
			return ERR_IOMETHOD;
		}
		break;
	}

	cam->cropcap->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (0 == _xioctl(cam->fd, VIDIOC_CROPCAP, cam->cropcap)) {
		cam->crop->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		cam->crop->c = cam->cropcap->defrect; /* reset to default */
		/*if (-1 == _xioctl(cam->fd, VIDIOC_S_CROP, cam.crop)) */
	}

	cam->fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (force_format) {
		cam->fmt->fmt.pix.width = 1280;
		cam->fmt->fmt.pix.height = 720;
		cam->fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
		cam->fmt->fmt.pix.field = V4L2_FIELD_INTERLACED;

		if (-1 == _xioctl(cam->fd, VIDIOC_S_FMT, cam->fmt))
			return ERR_VIDIOCSFMT;

		/* Note VIDIOC_S_FMT may change width and height. */
	} else {
		/* Preserve original settings as set by v4l2-ctl for example */
		if (-1 == _xioctl(cam->fd, VIDIOC_G_FMT, cam->fmt))
			return ERR_VIDIOCGFMT;
	}

	/* Buggy driver paranoia. */
	min = cam->fmt->fmt.pix.width * 2;
	if (cam->fmt->fmt.pix.bytesperline < min)
		cam->fmt->fmt.pix.bytesperline = min;
	min = cam->fmt->fmt.pix.bytesperline * cam->fmt->fmt.pix.height;
	if (cam->fmt->fmt.pix.sizeimage < min)
		cam->fmt->fmt.pix.sizeimage = min;
	
	cam_catcherror(__init[cam->iomethod](cam, cam->fmt->fmt.pix.sizeimage));
	
	return 0;
}

int cam_setdevname(camdevice * cam, char * name){
	cam->name =  malloc(strlen(name)+1);
	if (cam->name == NULL){
		return ERR_MALLOC;
	}
	strcpy(cam->name, name);
	return 0;
}

/*iomethod must be one of IO_METHOD_READ, IO_METHOD_MMAP, IO_METHOD_USERPTR,*/
int cam_setiomethod(camdevice * cam, int iomethod){
	cam->iomethod = iomethod;
	return 0;
}

int cam_setbuffer(camdevice * cam, buffer_t * b){
	/*Memory for the buffer b must be a reference to the heap*/
	cam->buf = b;
	return 0;
}

int cam_startcapturing(camdevice * cam){
	unsigned int i;
	enum v4l2_buf_type type;

	switch (cam->iomethod) {
	case IO_METHOD_READ:
		/* Nothing to do. */
		break;
	case IO_METHOD_MMAP:
		for (i = 0; i < cam->n_buf; ++i) {
			cam->v4lbuf = calloc(1, sizeof(struct v4l2_buffer));
			cam->v4lbuf->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			cam->v4lbuf->memory = V4L2_MEMORY_MMAP;
			cam->v4lbuf->index = i;
			if (-1 == _xioctl(cam->fd, VIDIOC_QBUF, cam->v4lbuf))
				return ERR_VIDIOQBUF;
		}
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (-1 == _xioctl(cam->fd, VIDIOC_STREAMON, &type))
			return ERR_VIDIOSTREAMON;
		break;

	case IO_METHOD_USERPTR:
		for (i = 0; i < cam->n_buf; ++i) {
			cam->v4lbuf = calloc(1, sizeof(struct v4l2_buffer));
			cam->v4lbuf->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			cam->v4lbuf->memory = V4L2_MEMORY_USERPTR;
			cam->v4lbuf->index = i;
			cam->v4lbuf->m.userptr = (unsigned long)cam->buf[i].start;
			cam->v4lbuf->length = cam->buf[i].length;
			if (-1 == _xioctl(cam->fd, VIDIOC_QBUF, cam->v4lbuf))
				return ERR_VIDIOQBUF;
		}
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (-1 == _xioctl(cam->fd, VIDIOC_STREAMON, &type))
			return ERR_VIDIOSTREAMON;
		break;
	}
	return 0;
}
int cam_stopcapturing(camdevice * cam){
	enum v4l2_buf_type type;

	switch (cam->iomethod) {
	case IO_METHOD_READ:
		/* Nothing to do. */
		break;
	case IO_METHOD_MMAP:
	case IO_METHOD_USERPTR:
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (-1 == _xioctl(cam->fd, VIDIOC_STREAMOFF, &type))
			return ERR_VIDIOSTREAMOFF;
		break;
	}
	return 0;
}
int cam_uninit(camdevice * cam){
	unsigned int i;
	switch (cam->iomethod) {
        case IO_METHOD_READ:
			free(cam->buf[0].start);
            break;

        case IO_METHOD_MMAP:
			for (i = 0; i < cam->n_buf; ++i)
				if (-1 == munmap(cam->buf[i].start, cam->buf[i].length))
					return ERR_MUNMAP;
			free(cam->v4lbuf);
			break;

        case IO_METHOD_USERPTR:
			for (i = 0; i < cam->n_buf; ++i)
				free(cam->buf[i].start);
			free(cam->v4lbuf);
            break;
        }

	free(cam->buf);
	free(cam->cap);
	free(cam->cropcap);
	free(cam->crop);
	free(cam->fmt);
	return 0;
}

int cam_catcherror(int err){
	char * msgerror[] = {
/*0*/	"No error detected",
		"It's not a video device",
		"Can't alloc",
		"Can't open device",
/*4*/	"Can't close device",
		"Can't query capabilities, is the device a v4l2 device?",
		"Method not supported by the device",
		"Error setting video format VIDIOC_S_FMT at ioctl",
		"Error getting video format VIDIOC_G_FMT at ioctl",
/*9*/	"Error requesting v4l2 buffer VIDIOC_REQBUFS at ioctl",
		"Not enough memory to satisfy the requirement",
		"Error querying the buffer VIDIOC_QUERYBUF at ioctl",
		"Error mapping memory in mmap() call",
		"Error VIDIOC_QBUF at ioctl",
		"Error setting the device for streaming VIDIOC_STREAMON at ioctl",
		"Error setting the device for streaming VIDIOC_STREAMOFF at ioctl",
		"Error unmapping memory in munmap() call",
		NULL
	};
	printf("%s\n", msgerror[err]);
	return err;
}