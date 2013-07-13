/*
 * PROJECT:     ReactOS Universal Serial Bus Bulk Enhanced Host Controller Interface
 * LICENSE:     GPL - See COPYING in the top level directory
 * FILE:        drivers/usb/usbccgp/usbccgp.c
 * PURPOSE:     USB  device driver.
 * PROGRAMMERS:
 *              Michael Martin (michael.martin@reactos.org)
 *              Johannes Anderwald (johannes.anderwald@reactos.org)
 *              Cameron Gutman
 */

#include "usbccgp.h"

/* Driver verifier */
DRIVER_ADD_DEVICE USBCCGP_AddDevice;

NTSTATUS
NTAPI
USBCCGP_AddDevice(
    PDRIVER_OBJECT DriverObject,
    PDEVICE_OBJECT PhysicalDeviceObject)
{
    NTSTATUS Status;
    PDEVICE_OBJECT DeviceObject;
    PFDO_DEVICE_EXTENSION FDODeviceExtension;

    /* Lets create the device */
    Status = IoCreateDevice(DriverObject,
                            sizeof(FDO_DEVICE_EXTENSION),
                            NULL,
                            FILE_DEVICE_USB,
                            FILE_AUTOGENERATED_DEVICE_NAME,
                            FALSE,
                            &DeviceObject);
    if (!NT_SUCCESS(Status))
    {
        /* Failed to create device */
        DPRINT1("USBCCGP_AddDevice failed to create device with %x\n", Status);
        return Status;
    }

    /* Get device extension */
    FDODeviceExtension = (PFDO_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    /* Init device extension */
    RtlZeroMemory(FDODeviceExtension, sizeof(FDO_DEVICE_EXTENSION));
    FDODeviceExtension->Common.IsFDO = TRUE;
    FDODeviceExtension->DriverObject = DriverObject;
    FDODeviceExtension->PhysicalDeviceObject = PhysicalDeviceObject;
    InitializeListHead(&FDODeviceExtension->ResetPortListHead);
    InitializeListHead(&FDODeviceExtension->CyclePortListHead);
    KeInitializeSpinLock(&FDODeviceExtension->Lock);

    FDODeviceExtension->NextDeviceObject = IoAttachDeviceToDeviceStack(DeviceObject,
                                                                       PhysicalDeviceObject);
    if (!FDODeviceExtension->NextDeviceObject)
    {
        /* Failed to attach */
        DPRINT1("USBCCGP_AddDevice failed to attach device\n");
        IoDeleteDevice(DeviceObject);
        return STATUS_DEVICE_REMOVED;
    }

    /* Set device flags */
    DeviceObject->Flags |= DO_BUFFERED_IO | DO_POWER_PAGABLE;

    /* Device is initialized */
    DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    /* Device initialized */
    return Status;
}

NTSTATUS
NTAPI
USBCCGP_CreateClose(
    PDEVICE_OBJECT DeviceObject, 
    PIRP Irp)
{
    PCOMMON_DEVICE_EXTENSION DeviceExtension;
    PFDO_DEVICE_EXTENSION FDODeviceExtension;

    /* Get common device extension */
    DeviceExtension = (PCOMMON_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    /* Is it a fdo */
    if (DeviceExtension->IsFDO)
    {
        /* Forward and forget */
        IoSkipCurrentIrpStackLocation(Irp);

        /* Get fdo */
        FDODeviceExtension = (PFDO_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

        /* Call lower driver */
        return IoCallDriver(FDODeviceExtension->NextDeviceObject, Irp);
    }
    else
    {
        /* Pdo not supported */
        Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_NOT_SUPPORTED;
    }
}

NTSTATUS
NTAPI
USBCCGP_Dispatch(
    PDEVICE_OBJECT DeviceObject, 
    PIRP Irp)
{
    PCOMMON_DEVICE_EXTENSION DeviceExtension;
    PIO_STACK_LOCATION IoStack;

    /* Get common device extension */
    DeviceExtension = (PCOMMON_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    /* Get current stack location */
    IoStack = IoGetCurrentIrpStackLocation(Irp);

    if (IoStack->MajorFunction == IRP_MJ_CREATE || IoStack->MajorFunction == IRP_MJ_CLOSE)
    {
        /* Dispatch to default handler */
        return USBCCGP_CreateClose(DeviceObject, Irp);
    }

    if (DeviceExtension->IsFDO)
    {
        /* Handle request for FDO */
        return FDO_Dispatch(DeviceObject, Irp);
    }
    else
    {
        /* Handle request for PDO */
        return PDO_Dispatch(DeviceObject, Irp);
    }
}

VOID
NTAPI
USBCCGP_Unload(PDRIVER_OBJECT DriverObject)
{
    DPRINT("[USBCCGP] Unload\n");
}

NTSTATUS
NTAPI
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath)
{

    /* Initialize driver object */
    DPRINT("[USBCCGP] DriverEntry\n");
    DriverObject->DriverExtension->AddDevice = USBCCGP_AddDevice;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = USBCCGP_Dispatch;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = USBCCGP_Dispatch;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = USBCCGP_Dispatch;
    DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = USBCCGP_Dispatch;
    DriverObject->MajorFunction[IRP_MJ_POWER] = USBCCGP_Dispatch;
    DriverObject->MajorFunction[IRP_MJ_PNP] = USBCCGP_Dispatch;
    DriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL] = USBCCGP_Dispatch;
    DriverObject->DriverUnload = USBCCGP_Unload;

    /* FIMXE query GenericCompositeUSBDeviceString */

    return STATUS_SUCCESS;
}
