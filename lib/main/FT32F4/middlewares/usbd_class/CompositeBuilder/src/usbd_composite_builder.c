/**
  ******************************************************************************
  * @file    			usbd_composite_builder.c
  * @author  			FMD XA
  * @brief   			This file provides all the composite builder functions.
  * @version 			V1.0.0           
  * @data		 			2025-04-21
  ******************************************************************************
  *
  * =========================================================
  *             Composite Builder Description
  * =========================================================
  * The composite builder builds the configuration descriptors based on the selection
  * of classes by user.
  * It includes all USB Device classes in order to instantiate their descriptors,
  * but for better management, it is possible to optimize footprint by removing
  * unused classes. It is possible to do so by commenting the relative define in
  * usbd_conf.h
  */

/* Includes ------------------------------------------------------------------*/
#include "usbd_composite_builder.h"

#ifdef USE_USBD_COMPOSITE

/**@defgroup CMPSIT_CORE
 * @{
 */

/**@defgroup CMPSIT_CORE_Private_TypeDefinitions
 * @{
 */

/**
 * @}
 */


/**@defgroup CMPSIT_CORE_Private_Defines
 * @{
 */

/**
 * @}
 */

/**@defgroup CMPSIT_CORE_Private_Macros
 * @{
 */
/**
 * @}
 */

/**@defgroup CMPSIT_CORE_Private_FunctionPrototypes
 * @{
 */
uint8_t *USBD_CMPSIT_GetFSCfgDesc(uint16_t *length);
#ifdef USE_USB_HS
uint8_t *USBD_CMPSIT_GetHSCfgDesc(uint16_t *length);
#endif /* USE_USB_HS */

uint8_t *USBD_CMPSIT_GetOtherSpeedCfgDesc(uint16_t *length);
uint8_t *USBD_CMPSIT_GetDeviceQualifierDescriptor(uint16_t *length);

static uint8_t USBD_CMPSIT_FindFreeIFNbr(USBD_HandleTypeDef *pdev);
static void    USBD_CMPSIT_AddConfDesc(uint32_t Conf, __IO uint32_t *pSze);
static void    USBD_CMPSIT_AssignEp(USBD_HandleTypeDef *pdev, uint8_t Add, uint8_t Type, uint32_t Sze);

#if USBD_CMPSIT_ACTIVATE_HID == 1U
static void USBD_CMPSIT_HIDMouseDesc(USBD_HandleTypeDef *pdev, uint32_t pConf, __IO uint32_t *Sze, uint8_t speed);
#endif  /* USBD_CMPSIT_ACTIVATE_HID */

#if USBD_CMPSIT_ACTIVATE_CDC == 1U
static void USBD_CMPSIT_CDCDesc(USBD_HandleTypeDef *pdev, uint32_t pConf, __IO uint32_t *Sze, uint8_t speed);
#endif  /* USBD_CMPSIT_ACTIVATE_CDC */

#if USBD_CMPSIT_ACTIVATE_MSC == 1U
static void USBD_CMPSIT_MSCDesc(USBD_HandleTypeDef *pdev, uint32_t pConf, __IO uint32_t *Sze, uint8_t speed);
#endif  /* USBD_CMPSIT_ACTIVATE_MSC */


/**
 * @}
 */

/**@defgroup CMPSIT_CORE_Private_Variables
 * @{
 */
/* this structure is used only for the configuration descriptors and device qualifier */
USBD_ClassTypeDef USBD_CMPSIT =
{
  NULL, /* Init */
  NULL, /* DeInit */
  NULL, /* Setup */
  NULL, /* EP0_TxSent */
  NULL, /* EP0_RxReady */
  NULL, /* DataIn */
  NULL, /* DataOut */
  NULL, /* SOF */
  NULL,
  NULL,
#ifdef USE_USB_HS
  USBD_CMPSIT_GetHSCfgDesc,
#else
  NULL,
#endif  /* USE_USB_HS */
  USBD_CMPSIT_GetFSCfgDesc,
  USBD_CMPSIT_GetOtherSpeedCfgDesc,
  USBD_CMPSIT_GetDeviceQualifierDescriptor,
#if (USBD_SUPPORT_USER_STRING_DESC == 1U)
  NULL,
#endif /* USBD_SUPPORT_USER_STRING_DESC */
};

/* The generic configuration descriptor buffer that will be filled by builder
 * size of the buffer is the maximum possible configuration descriptor size */
__ALIGN_BEGIN static uint8_t USBD_CMPSIT_FSCfgDesc[USBD_CMPST_MAX_CONFDESC_SZ] __ALIGN_END = {0};
static uint8_t *pCmpstFSConfDesc = USBD_CMPSIT_FSCfgDesc;
/* variable that dynamically holds the current size of the configuration descriptor */
static __IO uint32_t CurrFSConfDescSz = 0U;

#ifdef USE_USB_HS
__ALIGN_BEGIN static uint8_t USBD_CMPSIT_HSCfgDesc[USBD_CMPST_MAX_CONFDESC_SZ] __ALIGN_END = {0};
static uint8_t *pCmpstHSConfDesc = USBD_CMPSIT_HSCfgDesc;
/* variable that dynamically holds the current size of the configuration descriptor */
static __IO uint32_t CurrHSConfDescSz = 0U;
#endif /* USE_USB_HS */

/* USB Standard device descriptor */
__ALIGN_BEGIN static uint8_t USBD_CMPSIT_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END =
{
  USB_LEN_DEV_QUALIFIER_DESC,     /* bLength */
  USB_DESC_TYPE_DEVICE_QUALIFIER, /* bDescriptorType */
  0x00,                           /* bcdDevice low */
  0x02,                           /* bcdDevice high */
  0xEF,                           /* Class */
  0x02,                           /* SubClass */
  0x01,                           /* Protocol */
  0x40,                           /* bMaxPacketSize0 */
  0x01,                           /* bNumConfigurations */
  0x00,                           /* bReserved */
};

/**
 * @}
 */

/**@defgroup CMPSIT_CORE_Private_Functions
 * @{
 */

/**
  * @breif USBD_CMPSIT_AddClass
  *        register a class in the class builder
  * @param pdev: device instance
  * @param pclass: pointer to the class structure to be added
  * @param class: type of the class to be added (from USBD_CompositeClassTypeDef)
  * @param cfgidx: configuration index(unused)
  * @retval status
  */
uint8_t USBD_CMPSIT_AddClass(USBD_HandleTypeDef *pdev, USBD_ClassTypeDef *pclass,
                             USBD_CompositeClassTypeDef class)
{
  uint16_t savedFSSze;
#ifdef USE_USB_HS
  uint16_t savedHSSze;
#endif /* USE_USB_HS */

  /* Reject invalid inputs and an already-active or out-of-range slot before
   * mutating any state, so a duplicate or overflow registration cannot leave
   * a partial-active slot reported as success. */
  if ((pdev == NULL) || (pclass == NULL))
  {
    return (uint8_t)USBD_FAIL;
  }

  if (!((pdev->classId < USBD_MAX_SUPPORTED_CLASS) && (pdev->tclasslist[pdev->classId].Active == 0U)))
  {
    return (uint8_t)USBD_FAIL;
  }

  /* snapshot the descriptor sizes so a failed AddToConfDesc can be rolled back */
  savedFSSze = CurrFSConfDescSz;
#ifdef USE_USB_HS
  savedHSSze = CurrHSConfDescSz;
#endif /* USE_USB_HS */

  /* store the class parameters in the global tab */
  pdev->pClass[pdev->classId] = pclass;
  pdev->tclasslist[pdev->classId].ClassId   = pdev->classId;
  pdev->tclasslist[pdev->classId].Active    = 1U;
  pdev->tclasslist[pdev->classId].ClassType = class;

  /* call configuration descriptor builder and endpoint configuration builder*/
  if (USBD_CMPSIT_AddToConfDesc(pdev) != (uint8_t)USBD_OK)
  {
    /* roll back the partial slot and descriptor size so the failed class
     * leaves no trace in pClass/Active/ClassType or the config descriptor. */
    pdev->pClass[pdev->classId] = NULL;
    pdev->tclasslist[pdev->classId].Active    = 0U;
    pdev->tclasslist[pdev->classId].ClassType = (USBD_CompositeClassTypeDef)0U;
    CurrFSConfDescSz = savedFSSze;
#ifdef USE_USB_HS
    CurrHSConfDescSz = savedHSSze;
#endif /* USE_USB_HS */
    return (uint8_t)USBD_FAIL;
  }
  return (uint8_t)USBD_OK;
}

/**
  * @breif USBD_CMPSIT_AddToConfDesc
  *        Add a new class to the configuration descriptor
  * @param pdev: device instance
  * @retval status
  */
uint8_t USBD_CMPSIT_AddToConfDesc(USBD_HandleTypeDef *pdev)
{
  uint8_t idxIf = 0U;
  uint8_t iEp = 0U;

  /* for the first class instance, start building the config descriptor common part */
  if (pdev->classId == 0U)
  {
    /* Add configuration and IAD descriptors */
    USBD_CMPSIT_AddConfDesc((uint32_t)pCmpstFSConfDesc, &CurrFSConfDescSz);
#ifdef USE_USB_HS
    USBD_CMPSIT_AddConfDesc((uint32_t)pCmpstHSConfDesc, &CurrHSConfDescSz);
#endif /* USE_USB_HS */
  }

  switch (pdev->tclasslist[pdev->classId].ClassType)
  {
#if USBD_CMPSIT_ACTIVATE_HID == 1U
    case CLASS_TYPE_HID:
      /* setup max packet sizes (for HID, no dependency on USB Speed, both HS/FS have same packet size) */
      pdev->tclasslist[pdev->classId].CurrPcktSze = HID_EPIN_SIZE;

      /* find the first available interface slot and assign number of interface */
      idxIf = USBD_CMPSIT_FindFreeIFNbr(pdev);
      pdev->tclasslist[pdev->classId].NumIf  = 1U;
      pdev->tclasslist[pdev->classId].Ifs[0] = idxIf;

      /* assign endpoint numbers */
      pdev->tclasslist[pdev->classId].NumEps = 1U; /* EP1_IN */
      /* set in endpoint slot */
      iEp = pdev->tclasslist[pdev->classId].EpAdd[0];

      /* assign IN endpoint */
      USBD_CMPSIT_AssignEp(pdev, iEp, USBD_EP_TYPE_INTR, pdev->tclasslist[pdev->classId].CurrPcktSze);

      /* configure and Append the Descriptor */
      USBD_CMPSIT_HIDMouseDesc(pdev, (uint32_t)pCmpstFSConfDesc, &CurrFSConfDescSz, (uint8_t)USBD_SPEED_FULL);
#ifdef USE_USB_HS
      USBD_CMPSIT_HIDMouseDesc(pdev, (uint32_t)pCmpstHSConfDesc, &CurrHSConfDescSz, (uint8_t)USBD_SPEED_HIGH);
#endif /* USE_USB_HS */

      break;
#endif /* USBD_CMPSIT_ACTIVATE_HID */

#if USBD_CMPSIT_ACTIVATE_CDC == 1U
    case CLASS_TYPE_CDC:
      /* CDC uses two interfaces (communication + data) and three endpoints:
       * bulk IN, bulk OUT and interrupt IN (command). */
      pdev->tclasslist[pdev->classId].CurrPcktSze = CDC_DATA_FS_MAX_PACKET_SIZE;

      /* find the first available interface slot, assign communication and data interfaces */
      idxIf = USBD_CMPSIT_FindFreeIFNbr(pdev);
      pdev->tclasslist[pdev->classId].NumIf  = 2U;
      pdev->tclasslist[pdev->classId].Ifs[0] = idxIf;      /* communication interface */
      pdev->tclasslist[pdev->classId].Ifs[1] = (uint8_t)(idxIf + 1U); /* data interface */

      /* assign endpoint numbers: bulk IN, bulk OUT, interrupt IN */
      pdev->tclasslist[pdev->classId].NumEps = 3U;
      USBD_CMPSIT_AssignEp(pdev, pdev->tclasslist[pdev->classId].EpAdd[0],
                           USBD_EP_TYPE_BULK, CDC_DATA_FS_MAX_PACKET_SIZE);
      USBD_CMPSIT_AssignEp(pdev, pdev->tclasslist[pdev->classId].EpAdd[1],
                           USBD_EP_TYPE_BULK, CDC_DATA_FS_MAX_PACKET_SIZE);
      USBD_CMPSIT_AssignEp(pdev, pdev->tclasslist[pdev->classId].EpAdd[2],
                           USBD_EP_TYPE_INTR, CDC_CMD_PACKET_SIZE);

      /* configure and append the descriptor */
      USBD_CMPSIT_CDCDesc(pdev, (uint32_t)pCmpstFSConfDesc, &CurrFSConfDescSz, (uint8_t)USBD_SPEED_FULL);
#ifdef USE_USB_HS
      USBD_CMPSIT_CDCDesc(pdev, (uint32_t)pCmpstHSConfDesc, &CurrHSConfDescSz, (uint8_t)USBD_SPEED_HIGH);
#endif /* USE_USB_HS */

      break;
#endif /* USBD_CMPSIT_ACTIVATE_CDC */

#if USBD_CMPSIT_ACTIVATE_MSC == 1U
    case CLASS_TYPE_MSC:
      /* MSC uses a single interface with two bulk endpoints (IN and OUT). */
      pdev->tclasslist[pdev->classId].CurrPcktSze = MSC_MAX_FS_PACKET;

      idxIf = USBD_CMPSIT_FindFreeIFNbr(pdev);
      pdev->tclasslist[pdev->classId].NumIf  = 1U;
      pdev->tclasslist[pdev->classId].Ifs[0] = idxIf;

      /* assign endpoint numbers: bulk IN, bulk OUT */
      pdev->tclasslist[pdev->classId].NumEps = 2U;
      USBD_CMPSIT_AssignEp(pdev, pdev->tclasslist[pdev->classId].EpAdd[0],
                           USBD_EP_TYPE_BULK, MSC_MAX_FS_PACKET);
      USBD_CMPSIT_AssignEp(pdev, pdev->tclasslist[pdev->classId].EpAdd[1],
                           USBD_EP_TYPE_BULK, MSC_MAX_FS_PACKET);

      /* configure and append the descriptor */
      USBD_CMPSIT_MSCDesc(pdev, (uint32_t)pCmpstFSConfDesc, &CurrFSConfDescSz, (uint8_t)USBD_SPEED_FULL);
#ifdef USE_USB_HS
      USBD_CMPSIT_MSCDesc(pdev, (uint32_t)pCmpstHSConfDesc, &CurrHSConfDescSz, (uint8_t)USBD_SPEED_HIGH);
#endif /* USE_USB_HS */

      break;
#endif /* USBD_CMPSIT_ACTIVATE_MSC */

    default:
      /* Unsupported or disabled class type: no descriptor is appended. Report
       * failure so the caller rolls back the partial slot and does not
       * advertise a configuration that omits this class. */
      return (uint8_t)USBD_FAIL;
  }
  return (uint8_t)USBD_OK;
}

/**
  * @breif USBD_CMPSIT_GetFSCfgDesc
  *        return configuration descriptor for both FS and HS modes
  * @param length: pointer data length
  * @retval pointer ro descriptor buffer
  */
uint8_t *USBD_CMPSIT_GetFSCfgDesc(uint16_t *length)
{
  *length = (uint16_t)CurrFSConfDescSz;
  return USBD_CMPSIT_FSCfgDesc;
}

#ifdef USE_USB_HS
/**
  * @breif USBD_CMPSIT_GetHSCfgDesc
  *        return configuration descriptor for both FS and HS modes
  * @param length: pointer data length
  * @retval pointer ro descriptor buffer
  */
uint8_t *USBD_CMPSIT_GetHSCfgDesc(uint16_t *length)
{
  *length = (uint16_t)CurrHSConfDescSz;
  return USBD_CMPSIT_HSCfgDesc;
}
#endif /* USE_USB_HS */

/**
  * @breif USBD_CMPSIT_GetOtherSpeedCfgDesc
  *        return other speed configuration descriptor
  * @param length: pointer data length
  * @retval pointer ro descriptor buffer
  */
uint8_t *USBD_CMPSIT_GetOtherSpeedCfgDesc(uint16_t *length)
{
  *length = (uint16_t)CurrFSConfDescSz;
  return USBD_CMPSIT_FSCfgDesc;
}

/**
  * @breif USBD_CMPSIT_GetDeviceQualifierDescriptor
  *        return other speed configuration descriptor
  * @param length: pointer data length
  * @retval pointer ro descriptor buffer
  */
uint8_t *USBD_CMPSIT_GetDeviceQualifierDescriptor(uint16_t *length)
{
  *length = (uint16_t)(sizeof(USBD_CMPSIT_DeviceQualifierDesc));
  return USBD_CMPSIT_DeviceQualifierDesc;
}
/**
  * @breif USBD_CMPSIT_FindFreeIFNbr
  *        Find the first interface available slot
  * @param pdev: device instance
  * @retval the interface number to be used
  */
static uint8_t USBD_CMPSIT_FindFreeIFNbr(USBD_HandleTypeDef *pdev)
{
  uint32_t idx = 0U;

  /* unroll all already activated classes */
  for (uint32_t i = 0U; i < pdev->NumClasses; i++)
  {
    /* unroll each class interfaces */
    for (uint32_t j = 0U; j < pdev->tclasslist[i].NumIf; j++)
    {
      /* increment the interface counter index */
      idx++;
    }
  }
  /* return the first available interface slot */
  return (uint8_t)idx;
}


/**
  * @breif USBD_CMPSIT_AddConfDesc
  *        Add a new class to the configuration descriptor
  * @param pdev: device instance
  * @retval none
  */
static void USBD_CMPSIT_AddConfDesc(uint32_t Conf, __IO uint32_t *pSze)
{
  /* Intermediate varible to comply with MISRA-C Rule 11.3 */
  USBD_ConfigDescTypeDef *ptr = (USBD_ConfigDescTypeDef *)Conf;

  ptr->bLength = (uint8_t)sizeof(USBD_ConfigDescTypeDef);
  ptr->bDescriptorType = USB_DESC_TYPE_CONFIGURATION;
  ptr->wTotalLength = 0U;
  ptr->bNumInterfaces = 0U;
  /* A configuration descriptor must advertise a non-zero configuration
   * value: value 0 means unconfigured. The host selects the advertised
   * value with SET_CONFIGURATION, and USBD_SetConfig treats 0 as a
   * deconfiguration. All standalone FT32 class descriptors use 1, so the
   * composite descriptor must do the same to enumerate reliably. */
  ptr->bConfigurationValue = 1U;
  ptr->iConfiguration = USBD_CONFIG_STR_DESC_IDX;

#if (USBD_SELF_POWERED == 1U)
  ptr->bmAttributes = 0xC0U;     /* bmAttributes: self powered according to user configuration */
#else
  ptr->bmAttributes = 0x80U;     /* bmAttributes: bus powered according to user configuration */
#endif /* USBD_SELF_POWERED */

  ptr->bMaxPower = USBD_MAX_POWER;
  *pSze += sizeof(USBD_ConfigDescTypeDef);
}

/**
  * @breif USBD_CMPSIT_AssignEp
  *        Assign an endpoint
  * @param pdev: device instance
  * @param add: endpoint address
  * @param Type: endpoint type
  * @param Sze: endpoint max packet size
  * @retval none
  */
static void USBD_CMPSIT_AssignEp(USBD_HandleTypeDef *pdev, uint8_t Add, uint8_t Type, uint32_t Sze)
{
  uint32_t idx = 0U;
  /* Find the first available endpoint slot */
  while (((idx < (pdev->tclasslist[pdev->classId]).NumEps) &&
         ((pdev->tclasslist[pdev->classId].Eps[idx].is_used) != 0U)))
  {
    /* increment the index */
    idx++;
  }
  /* configure the endpoint */
  pdev->tclasslist[pdev->classId].Eps[idx].add      = Add;
  pdev->tclasslist[pdev->classId].Eps[idx].type     = Type;
  pdev->tclasslist[pdev->classId].Eps[idx].size     = (uint8_t)Sze;
  pdev->tclasslist[pdev->classId].Eps[idx].is_used  = 1U;

}

#if USBD_CMPSIT_ACTIVATE_HID == 1U
/**
  * @breif USBD_CMPSIT_HIDMouseDesc
  *        Configure and append the HID Mouse descriptor
  * @param pdev: device instance
  * @param pConf: configuration descriptor pointer
  * @param Sze: pointer ro the current configuration descriptor size
  * @retval none
  */
static void USBD_CMPSIT_HIDMouseDesc(USBD_HandleTypeDef *pdev, uint32_t pConf, __IO uint32_t *Sze, uint8_t speed)
{
  static USBD_IfDescTypeDef *pIfDesc;
  static USBD_EpDescTypeDef *pEpDesc;
  static USBD_HIDDescTypeDef *pHidMouseDesc;

  /* append HID interface descriptor to configuration descriptor */
  __USBD_CMPSIT_SET_IF(pdev->tclasslist[pdev->classId].Ifs[0], 0U,
                       (uint8_t)(pdev->tclasslist[pdev->classId].NumEps), 0x03U, 0x00U, 0x00U, 0U);
  /* append HID Functional descriptor to configuration descriptor */
  pHidMouseDesc = ((USBD_HIDDescTypeDef *)(pConf + *Sze));
  pHidMouseDesc->blength = (uint8_t)sizeof(USBD_HIDDescTypeDef);
  pHidMouseDesc->bDescriptorType = HID_DESCRIPTOR_TYPE;
  pHidMouseDesc->bcdHID = 0x0111U;
  pHidMouseDesc->bCountryCode = 0x00U;
  pHidMouseDesc->bNumDescriptors = 0x01U;
  pHidMouseDesc->bHIDDescriptorType = 0x22U;
  pHidMouseDesc->wItemLength = HID_MOUSE_REPORT_DESC_SIZE;
  *Sze += (uint32_t)sizeof(USBD_HIDDescTypeDef);
  /* append endpoint descriptor to configuration descriptor */
  __USBD_CMPSIT_SET_EP(pdev->tclasslist[pdev->classId].Eps[0].add, USBD_EP_TYPE_INTR, HID_EPIN_SIZE,
                       HID_HS_BINTERVAL, HID_FS_BINTERVAL);
  /* update config descriptor and IAD descriptor */
  ((USBD_ConfigDescTypeDef *)pConf)->bNumInterfaces += 1U;
  ((USBD_ConfigDescTypeDef *)pConf)->wTotalLength = (uint16_t)(*Sze);
}
#endif /* USBD_CMPSIT_ACTIVATE_HID */

#if USBD_CMPSIT_ACTIVATE_CDC == 1U
/**
  * @brief  USBD_CMPSIT_CDCDesc
  *         Configure and append the CDC (communication + data) descriptor,
  *         prefixed by an Interface Association Descriptor.
  * @param  pdev: device instance
  * @param  pConf: configuration descriptor pointer
  * @param  Sze: pointer to the current configuration descriptor size
  * @param  speed: current device speed
  * @retval none
  */
static void USBD_CMPSIT_CDCDesc(USBD_HandleTypeDef *pdev, uint32_t pConf, __IO uint32_t *Sze, uint8_t speed)
{
  static USBD_IadDescTypeDef *pIadDesc;
  static USBD_IfDescTypeDef *pIfDesc;
  static USBD_EpDescTypeDef *pEpDesc;
  uint8_t commIf = pdev->tclasslist[pdev->classId].Ifs[0];
  uint8_t dataIf = pdev->tclasslist[pdev->classId].Ifs[1];
  uint8_t *pFunc;

  /* IAD groups the CDC communication and data interfaces as one function.
   * It is only needed when CDC is part of a multi-function composite; a
   * standalone CDC device enumerates fine without it. NumClasses reflects
   * classes already registered before this one. */
  if (pdev->NumClasses > 0U)
  {
    pIadDesc = ((USBD_IadDescTypeDef *)(pConf + *Sze));
    pIadDesc->bLength = (uint8_t)sizeof(USBD_IadDescTypeDef);
    pIadDesc->bDescriptorType = USB_DESC_TYPE_IAD;
    pIadDesc->bFirstInterface = commIf;
    pIadDesc->bInterfaceCount = 2U;
    pIadDesc->bFunctionClass = USB_CLASS_CODE_CDC;
    pIadDesc->bFunctionSubClass = 0x02U; /* Abstract Control Model */
    pIadDesc->bFunctionProtocol = 0x01U; /* Common AT commands */
    pIadDesc->iFunction = 0U;
    *Sze += (uint32_t)sizeof(USBD_IadDescTypeDef);
  }

  /* CDC communication interface: one interrupt IN endpoint */
  __USBD_CMPSIT_SET_IF(commIf, 0U, 1U, USB_CLASS_CODE_CDC, 0x02U, 0x01U, 0U);

  /* CDC functional descriptors are written as raw bytes (no struct types). */
  pFunc = ((uint8_t *)(pConf + *Sze));

  /* Header functional descriptor: bcdCDC = 1.10 */
  pFunc[0] = 0x05U; pFunc[1] = USBD_CDC_CS_INTERFACE; pFunc[2] = 0x00U;
  pFunc[3] = 0x10U; pFunc[4] = 0x01U;
  /* Call Management functional descriptor */
  pFunc[5] = 0x05U; pFunc[6] = USBD_CDC_CS_INTERFACE; pFunc[7] = 0x01U;
  pFunc[8] = 0x00U; pFunc[9] = dataIf;
  /* ACM functional descriptor */
  pFunc[10] = 0x04U; pFunc[11] = USBD_CDC_CS_INTERFACE; pFunc[12] = 0x02U;
  pFunc[13] = 0x02U;
  /* Union functional descriptor: master = comm, slave = data */
  pFunc[14] = 0x05U; pFunc[15] = USBD_CDC_CS_INTERFACE; pFunc[16] = 0x06U;
  pFunc[17] = commIf; pFunc[18] = dataIf;
  *Sze += 19U;

  /* Command interrupt IN endpoint (Eps[2]) */
  __USBD_CMPSIT_SET_EP(pdev->tclasslist[pdev->classId].Eps[2].add, USBD_EP_TYPE_INTR,
                       CDC_CMD_PACKET_SIZE, CDC_BINTERVAL, CDC_FS_BINTERVAL);

  /* CDC data interface: two bulk endpoints (OUT and IN) */
  __USBD_CMPSIT_SET_IF(dataIf, 0U, 2U, USB_CLASS_CODE_CDCDATA, 0x00U, 0x00U, 0U);

  /* Bulk OUT endpoint (Eps[1]) */
  __USBD_CMPSIT_SET_EP(pdev->tclasslist[pdev->classId].Eps[1].add, USBD_EP_TYPE_BULK,
                       CDC_DATA_FS_MAX_PACKET_SIZE, 0U, 0U);
  /* Bulk IN endpoint (Eps[0]) */
  __USBD_CMPSIT_SET_EP(pdev->tclasslist[pdev->classId].Eps[0].add, USBD_EP_TYPE_BULK,
                       CDC_DATA_FS_MAX_PACKET_SIZE, 0U, 0U);

  /* CDC contributes two interfaces to the configuration */
  ((USBD_ConfigDescTypeDef *)pConf)->bNumInterfaces += 2U;
  ((USBD_ConfigDescTypeDef *)pConf)->wTotalLength = (uint16_t)(*Sze);
}
#endif /* USBD_CMPSIT_ACTIVATE_CDC */

#if USBD_CMPSIT_ACTIVATE_MSC == 1U
/**
  * @brief  USBD_CMPSIT_MSCDesc
  *         Configure and append the Mass Storage interface descriptor.
  * @param  pdev: device instance
  * @param  pConf: configuration descriptor pointer
  * @param  Sze: pointer to the current configuration descriptor size
  * @param  speed: current device speed
  * @retval none
  */
static void USBD_CMPSIT_MSCDesc(USBD_HandleTypeDef *pdev, uint32_t pConf, __IO uint32_t *Sze, uint8_t speed)
{
  static USBD_IfDescTypeDef *pIfDesc;
  static USBD_EpDescTypeDef *pEpDesc;
  uint16_t pktSize = (speed == (uint8_t)USBD_SPEED_HIGH) ? MSC_MAX_HS_PACKET : MSC_MAX_FS_PACKET;

  /* MSC interface: SCSI transparent command set, bulk-only transport */
  __USBD_CMPSIT_SET_IF(pdev->tclasslist[pdev->classId].Ifs[0], 0U,
                       (uint8_t)(pdev->tclasslist[pdev->classId].NumEps),
                       USB_CLASS_CODE_MSC, 0x06U, 0x50U, 0U);

  /* Bulk IN endpoint (Eps[0]) */
  __USBD_CMPSIT_SET_EP(pdev->tclasslist[pdev->classId].Eps[0].add, USBD_EP_TYPE_BULK,
                       pktSize, 0U, 0U);
  /* Bulk OUT endpoint (Eps[1]) */
  __USBD_CMPSIT_SET_EP(pdev->tclasslist[pdev->classId].Eps[1].add, USBD_EP_TYPE_BULK,
                       pktSize, 0U, 0U);

  /* MSC contributes one interface to the configuration */
  ((USBD_ConfigDescTypeDef *)pConf)->bNumInterfaces += 1U;
  ((USBD_ConfigDescTypeDef *)pConf)->wTotalLength = (uint16_t)(*Sze);
}
#endif /* USBD_CMPSIT_ACTIVATE_MSC */

/**
  * @breif USBD_CMPSIT_SetClassID
  *        find and set the class ID relative to selected class type and instance
  * @param pdev: device instance
  * @param Class: class type, can be CLASS_TYPE_NONE if requested to find class from setup request
  * @param Instance: instance number of the class(0: if first/unique instance, >0: otherwise)
  * @retval the class id, the pdev->classId is set with the value of the selected class ID.
  */
uint32_t USBD_CMPSIT_SetClassID(USBD_HandleTypeDef *pdev, USBD_CompositeClassTypeDef Class,
                                uint32_t Instance)
{
  uint32_t idx;
  uint32_t inst = 0U;

  /* unroll all already activated classes */
  for (idx = 0U; idx < pdev->NumClasses; idx++)
  {
    /* check if the class correspond to the requested type and if it is active */
    if (((USBD_CompositeClassTypeDef)(pdev->tclasslist[idx].ClassType) == Class) &&
        ((pdev->tclasslist[idx].Active) == 1U))
    {
      if (inst == Instance)
      {
        /* set the new class ID */
        pdev->classId = idx;
        /* return the class ID value */
        return (idx);
      }
      else
      {
        /* increment instance index and look for next instance */
        inst++;
      }
    }
  }
  /* No class found, return 0xFF */
  return 0xFFU;
}

/**
  * @breif USBD_CMPSIT_GetClassID
  *        return the class ID relative to selected class type and instance
  * @param pdev: device instance
  * @param Class: class type, can be CLASS_TYPE_NONE if requested to find class from setup request
  * @param Instance: instance number of the class(0: if first/unique instance, >0: otherwise)
  * @retval the class id(this function does not set the pdev->classId field).
  */
uint32_t USBD_CMPSIT_GetClassID(USBD_HandleTypeDef *pdev, USBD_CompositeClassTypeDef Class,
                                uint32_t Instance)
{
  uint32_t idx;
  uint32_t inst = 0U;

  /* unroll all already activated classes */
  for (idx = 0U; idx < pdev->NumClasses; idx++)
  {
    /* check if the class correspond to the requested type and if it is active */
    if (((USBD_CompositeClassTypeDef)(pdev->tclasslist[idx].ClassType) == Class) &&
        ((pdev->tclasslist[idx].Active) == 1U))
    {
      if (inst == Instance)
      {
        /* return the class ID value */
        return (idx);
      }
      else
      {
        /* increment instance index and look for next instance */
        inst++;
      }
    }
  }
  /* No class found, return 0xFF */
  return 0xFFU;
}

/**
  * @breif USBD_CMPSIT_ClearConfDesc
  *        Reset the configuration descriptor
  * @param pdev: device instance
  * @retval Status
  */
uint8_t USBD_CMPSIT_ClearConfDesc(void)
{
  /* reset the configuration descriptor pointer to default value and its size to zero */
  pCmpstFSConfDesc = USBD_CMPSIT_FSCfgDesc;
  CurrFSConfDescSz = 0U;

#ifdef USE_USB_HS
  pCmpstHSConfDesc = USBD_CMPSIT_HSCfgDesc;
  CurrHSConfDescSz = 0U;
#endif  /* USE_USB_HS */

  return (uint8_t)USBD_OK;
}


#endif  /* USE_USBD_COMPOSITE */

/**
 * @}
 */



/**
 * @}
 */


/**
 * @}
 */


/************************ (C) COPYRIGHT FMD *****END OF FILE****/
