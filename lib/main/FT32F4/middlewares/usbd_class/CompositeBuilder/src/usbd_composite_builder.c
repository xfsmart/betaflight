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
  if ((pdev->classId < USBD_MAX_SUPPORTED_CLASS) && (pdev->tclasslist[pdev->classId].Active == 0U))
  {
    /* store the class parameters in the global tab */
    pdev->pClass[pdev->classId] = pclass;
    pdev->tclasslist[pdev->classId].ClassId   = pdev->classId;
    pdev->tclasslist[pdev->classId].Active    = 1U;
    pdev->tclasslist[pdev->classId].ClassType = class;

    /* call configuration descriptor builder and endpoint configuration builder*/
    if (USBD_CMPSIT_AddToConfDesc(pdev) != (uint8_t)USBD_OK)
    {
      return (uint8_t)USBD_FAIL;
    }
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
#if USBD_CMPSIT_ACTIVE_HID == 1
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
#endif /* USBD_CMPSIT_ACTIVE_HID */

    default:
      break;
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
  ptr->bConfigurationValue = 0U;
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

#if USBD_CMPSIT_ACTIVE_HID == 1
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
                       (uint8_t)(pdev->tclasslist[pdev->classId].NumEps), 0x03U, 0x01U, 0x02U, 0U);
  /* append HID Functional descriptor to configuration descriptor */
  pHidMouseDesc = ((USBD_HIDDescTypeDef *)(pConf + *Sze));
  pHidMouseDesc->bLength = (uint8_t)sizeof(USBD_HIDDescTypeDef);
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
#endif /* USBD_CMPSIT_ACTIVE_HID */

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
