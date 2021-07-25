#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <xf86drm.h>
#include <xf86drmMode.h>


struct buffer_object {
	uint32_t width;
	uint32_t height;
	uint32_t pitch;
	uint32_t handle;
	uint32_t size;
	uint8_t *vaddr;
	uint32_t fb_id;
};

struct buffer_object buf;
int fd;
drmModeConnector *conn;
drmModeRes *res;
uint32_t conn_id;
uint32_t crtc_id;
unsigned int *mem;

static void write_color(struct buffer_object *bo,unsigned int color)
{
	unsigned int *pt;
	int i;
	
	pt = bo->vaddr;
	for (i = 0; i < (bo->size / 4); i++){
		*pt = color;
		pt++;
	}	
	
}

static void write_color_area(struct buffer_object *bo,unsigned int start,unsigned int end,unsigned int color)
{
	unsigned int *pt;
	int i;
	
	for (i = start; i < end; i++){
        pt = bo->vaddr + i;
		*pt = color;
	}
}

static unsigned int get_offset(struct buffer_object *bo,int x,int y){
    int linelen = bo->width*4;
    return (linelen*y) + (x*4);
}

static void draw_pixel(struct buffer_object *bo,unsigned int x,unsigned int y,unsigned int color)
{
	unsigned int *pt;
	int i;
	
	pt = bo->vaddr + get_offset(bo,x,y);
	*pt = color;
}

static unsigned int * modeset_create_fb(int fd, struct buffer_object *bo)
{
	struct drm_mode_create_dumb create = {};
 	struct drm_mode_map_dumb map = {};
	int i,ret;
	unsigned int *pt;

	create.width = bo->width;
	create.height = bo->height;
	create.bpp = 32;
	ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);
	if (ret < 0) {
		printf( "cannot create dumb buffer\n");
		return NULL;
		
	}

	bo->pitch = create.pitch;
	bo->size = create.size;
	bo->handle = create.handle;
	ret = drmModeAddFB(fd, bo->width, bo->height, 24, 32, bo->pitch,
			   bo->handle, &bo->fb_id);
	if (ret) {
		printf("cannot create framebuffer\n");
		return NULL;
	}
	
	map.handle = create.handle;
	drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map);

	bo->vaddr = mmap(0, create.size, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, map.offset);

	//memset(bo->vaddr, 0x0, bo->size);
	if(!bo->vaddr){
		printf("mmap err\n");
		return NULL;
	}

	pt = bo->vaddr;
	for (i = 0; i < (bo->size / 4); i++){
		*pt = 0x00ff0000;
		pt++;
	}
	
	
	return pt;
}

static void modeset_destroy_fb(int fd, struct buffer_object *bo)
{
	struct drm_mode_destroy_dumb destroy = {};

	drmModeRmFB(fd, bo->fb_id);

	munmap(bo->vaddr, bo->size);

	destroy.handle = bo->handle;
	drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}

struct buffer_object *init(char *dev)
{
	fd = open(dev, O_RDWR | O_CLOEXEC);

	res = drmModeGetResources(fd);
	crtc_id = res->crtcs[0];
	conn_id = res->connectors[0];

	conn = drmModeGetConnector(fd, conn_id);
	buf.width = conn->modes[0].hdisplay;
	buf.height = conn->modes[0].vdisplay;

	mem = modeset_create_fb(fd, &buf);

	drmModeSetCrtc(fd, crtc_id, buf.fb_id,
			0, 0, &conn_id, 1, &conn->modes[0]);
    return &buf;
}

void sc_close(){
    modeset_destroy_fb(fd, &buf);

	drmModeFreeConnector(conn);
	drmModeFreeResources(res);

	close(fd);
}

static void draw_line(struct buffer_object *bo,unsigned int x1,unsigned int y1,unsigned int x2,unsigned int y2,unsigned int color){
    if(x1>x2){
        int tmp = x2;
        x2=x1;
        x1=tmp;
        
        tmp = y2;
        y2=y1;
        y1=tmp;
    }
    if(x2==x1){
        for(int y=y1;y<y2;y++){
            draw_pixel(bo,x1,y,color);
        }
    }
    else
    {
        double k= ((double)(y2-y1)) / (x2-x1);
        for(int dx=0;dx<(x2-x1);dx++){
            int x = x1+dx;
            int y = y1+(int)(dx*k);
            draw_pixel(bo,x,y,color);
        }
    }
}

static void draw_rectangle(struct buffer_object *bo,unsigned int x1,unsigned int y1,unsigned int x2,unsigned int y2,unsigned int color){
    draw_line(bo,x1,y1,x2,y1,color);
    draw_line(bo,x1,y1,x1,y2,color);
    draw_line(bo,x1,y2,x2,y2,color);
    draw_line(bo,x2,y1,x2,y2,color);
}

static void fill_rectangle(struct buffer_object *bo,unsigned int x1,unsigned int y1,unsigned int x2,unsigned int y2,unsigned int color){
    for(int y=y1;y<y2;y++){
        int start = get_offset(bo,x1,y);
        int end = get_offset(bo,x2,y);
        write_color_area(bo,start,end,color);
    }
}

static void draw_circle(struct buffer_object *bo,unsigned int x,unsigned y,unsigned r,int color){
    int starty = y-r<0 ? 0:y-r;
    int endy = y+r>bo->height?bo->height:y+r;
    for(int i=starty;i<endy;i++){
        int dy = y-i;
        if(abs(dy)>=r)break;
        int dx = sqrt((r*r)-(dy*dy));
        if(abs(dy)+1 >= r)
        {
            draw_pixel(bo,x+dx,i,color);
            draw_pixel(bo,x-dx,i,color);
        }else{
            int dy1=dy+1;
            int dx1 = sqrt((r*r)-(dy1*dy1));
            draw_line(bo,x+dx,i,x+dx1,i+1,color);
            draw_line(bo,x-dx,i,x-dx1,i+1,color);
        }
    }
}

static void fill_circle(struct buffer_object *bo,unsigned int x,unsigned y,unsigned r,int color){
    int starty = y-r<0 ? 0:y-r;
    int endy = y+r>bo->height?bo->height:y+r;
    for(int i=starty;i<endy;i++){
        int dy = y-i;
        if(abs(dy)>=r)break;
        int dx = sqrt((r*r)-(dy*dy));
        int start = get_offset(bo,x-dx,i);
        int end = get_offset(bo,x+dx,i);
        write_color_area(bo,start,end,color);
    }
}

static void fill_bitmap(struct buffer_object *bo,uint8_t *bitmap){
    unsigned int *pt;
    pt = bo->vaddr;
    *pt = *bitmap;
}

static uint32_t width(struct buffer_object *bo){
    return bo->width;
}

static uint32_t height(struct buffer_object *bo){
    return bo->height;
}

void main(){
    struct buffer_object *b = init("/dev/dri/card0");
    fill_rectangle(b,25,25,100,100,0x00ffffff);
    while(1);
}