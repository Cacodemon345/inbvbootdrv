#define INBVSHIM_TYPE 40001

#define IOCTL_INBVSHIM_SOLID_COLOR_FILL \
	CTL_CODE( INBVSHIM_TYPE, 0x850, METHOD_IN_DIRECT, FILE_ANY_ACCESS )
	
#define IOCTL_INBVSHIM_RESET_DISPLAY \
	CTL_CODE( INBVSHIM_TYPE, 0x851, METHOD_NEITHER, FILE_ANY_ACCESS )
	
#define IOCTL_INBVSHIM_DISPSTRING_XY \
	CTL_CODE( INBVSHIM_TYPE, 0x852, METHOD_IN_DIRECT, FILE_ANY_ACCESS )