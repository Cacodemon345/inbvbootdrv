#include <ntddk.h>
#include <wdm.h>
#include "inbvshim.h"

#define NT_DEVICE_NAME      L"\\Device\\InbvShim"

static PDEVICE_OBJECT deviceObject = NULL;

typedef enum _INBV_DISPLAY_STATE
{
    INBV_DISPLAY_STATE_OWNED,
    INBV_DISPLAY_STATE_DISABLED,
    INBV_DISPLAY_STATE_LOST
} INBV_DISPLAY_STATE;

//
// Function Callbacks
//
typedef BOOLEAN (NTAPI *INBV_RESET_DISPLAY_PARAMETERS)(ULONG Cols, ULONG Rows);

typedef VOID (NTAPI *INBV_DISPLAY_STRING_FILTER)(PCHAR *Str);

VOID NTAPI InbvAcquireDisplayOwnership(VOID);

BOOLEAN NTAPI InbvCheckDisplayOwnership(VOID);

VOID NTAPI InbvNotifyDisplayOwnershipLost(IN INBV_RESET_DISPLAY_PARAMETERS Callback);

//
// Installation Functions
//
VOID NTAPI InbvEnableBootDriver(IN BOOLEAN Enable);

VOID NTAPI InbvInstallDisplayStringFilter(IN INBV_DISPLAY_STRING_FILTER DisplayFilter);

BOOLEAN NTAPI InbvIsBootDriverInstalled(VOID);

//
// Display Functions
//
BOOLEAN NTAPI InbvDisplayString(IN PCHAR String);

BOOLEAN NTAPI InbvEnableDisplayString(IN BOOLEAN Enable);

BOOLEAN NTAPI InbvResetDisplay(VOID);

VOID NTAPI InbvSetScrollRegion(IN ULONG Left, IN ULONG Top, IN ULONG Width, IN ULONG Height);

VOID NTAPI InbvSetTextColor(IN ULONG Color);

VOID NTAPI InbvSolidColorFill(IN ULONG Left, IN ULONG Top, IN ULONG Width,IN ULONG Height, IN ULONG Color);

DRIVER_INITIALIZE DriverEntry;

__drv_dispatchType(IRP_MJ_CREATE)
__drv_dispatchType(IRP_MJ_CLOSE)
DRIVER_DISPATCH InbvShimCreateClose;

__drv_dispatchType(IRP_MJ_DEVICE_CONTROL)
DRIVER_DISPATCH InbvShimDeviceControl;

DRIVER_UNLOAD InbvShimUnloadDriver;

void InbvShimUnloadDriver(PDRIVER_OBJECT pDriverObject)
{
	if (deviceObject) IoDeleteDevice(deviceObject);
	InbvDisplayString((UCHAR*)"InbvShim driver unloaded.\n");
}

BOOLEAN IsWindows8OrHigher()
{
	RTL_OSVERSIONINFOW verInfo;
	NTSTATUS verStatus;
	RtlZeroMemory(&verInfo, sizeof(RTL_OSVERSIONINFOW));
	verInfo.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);
	verStatus = RtlGetVersion(&verInfo);
	if (NT_SUCCESS(verStatus))
	{
		return verInfo.dwMajorVersion >= 6 && verInfo.dwMinorVersion >= 2;
	}
	return FALSE;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPath)
{
	NTSTATUS Status;
    ULONG BytesReturned;
	UNICODE_STRING ntUnicodeString;
	
	RtlInitUnicodeString(&ntUnicodeString, NT_DEVICE_NAME);
	Status = IoCreateDevice(pDriverObject, 0ul, &ntUnicodeString, FILE_DEVICE_UNKNOWN, 0, FALSE, &deviceObject);
	if (!NT_SUCCESS(Status))
	{
		return Status;
	}
	
    pDriverObject->MajorFunction[IRP_MJ_CREATE] = InbvShimCreateClose;
    pDriverObject->MajorFunction[IRP_MJ_CLOSE] = InbvShimCreateClose;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = InbvShimDeviceControl;
	pDriverObject->DriverUnload = InbvShimUnloadDriver;
	InbvEnableBootDriver(TRUE);
	InbvAcquireDisplayOwnership();
    InbvResetDisplay();
    InbvSetTextColor(9); // Light Red color.
    InbvInstallDisplayStringFilter(NULL);
    InbvEnableDisplayString(TRUE);
    InbvDisplayString((UCHAR*)"InbvShim driver loaded.\n");
    InbvSetScrollRegion(0, 0, 679, 449);

    return STATUS_SUCCESS;
}

NTSTATUS InbvShimCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest( Irp, IO_NO_INCREMENT );

    return STATUS_SUCCESS;
}

NTSTATUS InbvShimDeviceControl(PDEVICE_OBJECT devObj, PIRP Irp)
{
	PIO_STACK_LOCATION irpSp;
	ULONG inputBufferLength;
	PCHAR inputBuffer;
	PULONG inputBufferLong;
	NTSTATUS Status = STATUS_SUCCESS;
	UINT32 i;
	
	UNREFERENCED_PARAMETER(devObj);
	
	irpSp = IoGetCurrentIrpStackLocation(Irp);
	
	if (!irpSp)
	{
		Status = STATUS_INSUFFICIENT_RESOURCES;
		goto End;
	}
	
	inputBufferLength = irpSp->Parameters.DeviceIoControl.InputBufferLength;
	
	if (!inputBufferLength)
	{
		Status = STATUS_INVALID_PARAMETER;
		goto End;
	}
	
	switch(irpSp->Parameters.DeviceIoControl.IoControlCode)
	{
		case IOCTL_INBVSHIM_DISPSTRING_XY:
		{
			UCHAR* inBufferExtra;
			ULONG inBufferExtraLength;
			ULONG Height;
			
			inputBuffer = Irp->AssociatedIrp.SystemBuffer;
			Height = inputBuffer[1] + 16;
			if (inputBufferLength < sizeof(ULONG) * 2)
			{
				Status = STATUS_INVALID_PARAMETER;
				break;
			}
			InbvSetScrollRegion(inputBuffer[0], inputBuffer[1], 679, Height);
			inBufferExtra = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
			
			if (!inBufferExtra)
			{
				Status = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}
			inBufferExtraLength = MmGetMdlByteCount(Irp->MdlAddress);
			if (inBufferExtraLength == 0)
			{
				Status = STATUS_INVALID_PARAMETER;
				break;
			}
			for (i = 0; i < inBufferExtraLength; i++)
			{
				if (inBufferExtra[i] == (UCHAR)('\n'))
				{
					Height += 16;
					InbvSetScrollRegion(inputBuffer[0], inputBuffer[1], 679, Height);
				}
			}
			if (inBufferExtra[inBufferExtraLength - 1] != 0)
			{
				Status = STATUS_INVALID_PARAMETER;
				break;
			}
			InbvDisplayString(inBufferExtra);
			break;
		}
		case IOCTL_INBVSHIM_RESET_DISPLAY:
		{
			InbvResetDisplay();
			InbvSetTextColor(9); // Light Red color.
			InbvInstallDisplayStringFilter(NULL);
			InbvEnableDisplayString(TRUE);
			InbvSetScrollRegion(0, 0, 679, 449);
			break;
		}
		case IOCTL_INBVSHIM_SOLID_COLOR_FILL:
		{
			inputBuffer = Irp->AssociatedIrp.SystemBuffer;
			// inputBufferLength should be at least sizeof(ULONG) * 5.
			if (inputBufferLength < sizeof(ULONG) * 5)
			{
				Status = STATUS_INVALID_PARAMETER;
				break;
			}
			inputBufferLong = (PULONG)inputBuffer;
			if (IsWindows8OrHigher() && (inputBufferLong[0] != 639 || inputBufferLong[1] != 479 || inputBufferLong[2] != 0 || inputBufferLong[3] != 0))
			{
				Status = STATUS_NOT_SUPPORTED;
				break;
			}
			if (inputBufferLong[0] > 639)
			{
				Status = STATUS_INVALID_PARAMETER;
				break;
			}
			if (inputBufferLong[1] > 479)
			{
				Status = STATUS_INVALID_PARAMETER;
				break;
			}
			if (inputBufferLong[2] < inputBufferLong[0] || inputBufferLong[2] >= 640)
			{
				Status = STATUS_INVALID_PARAMETER;
				break;
			}
			if (inputBufferLong[3] < inputBufferLong[1] || inputBufferLong[3] >= 480)
			{
				Status = STATUS_INVALID_PARAMETER;
				break;
			}
			if (inputBufferLong[4] >= 16)
			{
				Status = STATUS_INVALID_PARAMETER;
				break;
			}
			InbvSolidColorFill(inputBufferLong[0], inputBufferLong[1], inputBufferLong[2], inputBufferLong[3], inputBufferLong[4]);
			break;
		}
		default:
		{
			Status = STATUS_INVALID_DEVICE_REQUEST;
			break;
		}
	}
End:
	 Irp->IoStatus.Status = Status;
	 IoCompleteRequest(Irp, IO_NO_INCREMENT);
	 return Status;
}