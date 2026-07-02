/**
  ******************************************************************************
  * @file    			usbd_core.c
  * @author  			FMD XA
  * @brief   			This file porvides all the USBD core function.
  * @version 			V1.0.0           
  * @data		 			2025-04-17
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "usbd_core.h"

#ifdef  USE_USBD_COMPOSITE
#include "usbd_composite_builder.h"
#endif  /* USE_USBD_COMPOSITE */

/**@defgroup USBD_CORE
 * @brief usbd core module.
 * @{
 */


/**
 * @brief  USBD_Init
 *         Initialize the device stack and load the class driver
 * @param  pdev: device Handle
 * @param  pdesc: descriptor structure address
 * @param  id: Low level core index
 * @retval USBD Status
 */

USBD_StatusTypeDef  USBD_Init(USBD_HandleTypeDef *pdev, uint8_t core_id, uint8_t id, USBD_ClassTypeDef *pclass, USBD_DescriptorsTypeDef *pdesc)
{
  USBD_StatusTypeDef ret;
  UNUSED(id);  /* Parameter not used in this implementation */
  /* check whether the usb device handle is valid */
  if (pdev == NULL)
  {
#if (USBD_DEBUG_LEVEL > 1U)
    USBD_ErrLog("Invalid Device handle");
#endif
    return USBD_FAIL;
  }

#ifdef  USE_USBD_COMPOSITE
  /* parse the table of classes in use */
  for (uint32_t i = 0; i < USBD_MAX_SUPPORTED_CLASS; i++)
  {
    /* unlink previous class */
    pdev->pClass[i] = NULL;
    pdev->pUserData[i] = NULL;

    /* set class as inactive */
    pdev->tclasslist[i].Active = 0;
    pdev->NumClasses = 0;
    pdev->classId    = 0;
  }
#else
  /* unlink previous class */
  pdev->pClass[0] = NULL;
  pdev->pUserData[0] = NULL;
#endif  /* USE_USBD_COMPOSITE */

  pdev->pConfDesc = NULL;
  /* assign usbd descriptors */
  if (pdesc != NULL)
  {
    pdev->pDesc = pdesc;
  }
  if (pclass != NULL)
  {
    pdev->pClass[0] = pclass;
  }
  /* set device initial state */
  pdev->dev_state = USBD_STATE_DEFAULT;
  pdev->id = 0;
  pdev->dev_speed = core_id;
  /* Initialize low level driver */
  ret = USBD_LL_Init(pdev);
    
  return ret;
}

/**
 * @brief  USBD_DeInit
 *         De-Initialize the device library.
 * @param  pdev: device instance
 * @retval USBD Status
 */
USBD_StatusTypeDef  USBD_DeInit(USBD_HandleTypeDef *pdev)
{
  USBD_StatusTypeDef ret;
  /* Disconnect the USB Device */
  (void)USBD_LL_Stop(pdev);
  /* set default state */
  pdev->dev_state = USBD_STATE_DEFAULT;

#ifdef  USE_USBD_COMPOSITE
  /* parse the table of classes in use */
  for (uint32_t i = 0; i < USBD_MAX_SUPPORTED_CLASS; i++)
  {
    /* check if current class is in use */
    if ((pdev->tclasslist[i].Active) == 1U)
    {
      if (pdev->pClass[i] != NULL)
      {
        pdev->classId = i;
        /* free class resources */
        pdev->pClass[i]->DeInit(pdev);
      }
    }
  }
#else
  /* free class resources */
  if (pdev->pClass[0] != NULL)
  {
    pdev->pClass[0]->DeInit(pdev);
  }

  pdev->pUserData[0] = NULL;

#endif /* USE_USBD_COMPOSITE */

  /* free device descriptors resources */
  pdev->pDesc = NULL;
  pdev->pConfDesc = NULL;

  /* DeInitialize low level driver */
  ret = USBD_LL_DeInit(pdev);

  return ret;
}

#ifdef USB_OTG_HS_CORE
void USBD_SET_ADDR_Callback(PCD_HS_HandleTypeDef *hpcd)
{
    USBD_SET_ADDR(hpcd->pData);
}
#endif /* USB_OTG_HS_CORE */

/**@defgroup USBD_CORE_Private_Functions
 * @{
 */
#ifdef USB_OTG_HS
void USBD_SET_ADDR(USBD_HandleTypeDef *pdev)
{

    if(pdev->dev_address != 0xFF)
    {
      (void)USBD_LL_SetUSBAddress(pdev, pdev->dev_address);

      if (pdev->dev_address != 0U)
      {
        pdev->dev_state = USBD_STATE_ADDRESSED;
      }
      else
      {
        pdev->dev_state = USBD_STATE_DEFAULT;
      }    
    }
  
    pdev->dev_address = 0xFF;  
}
#endif /* USB_OTG_HS */
#ifdef USB_OTG_FS
void USBD_SET_ADDR(USBD_HandleTypeDef *pdev)
{
  if(!(USB_FS->CSR0 & OTG_FS_CSR0_RXPKTRDY))
  {
    if(pdev->dev_address != 0xFF)
    {
      (void)USBD_LL_SetUSBAddress(pdev, pdev->dev_address);

      if (pdev->dev_address != 0U)
      {
        pdev->dev_state = USBD_STATE_ADDRESSED;
      }
      else
      {
        pdev->dev_state = USBD_STATE_DEFAULT;
      }    
    }
  }    
  pdev->dev_address = 0xFF;  
}
#endif /* USB_OTG_FS */
/**
 * @brief  USBD_RegisterClass
 *         Link class driver to device core.
 * @param  pdev: device handle
 * @param  pclass: class handle
 * @retval USBD Status
 */
USBD_StatusTypeDef  USBD_RegisterClass(USBD_HandleTypeDef *pdev, USBD_ClassTypeDef *pclass)
{
  uint16_t len = 0U;

  if (pclass == NULL)
  {
#if (USBD_DEBUG_LEVEL > 1U)
    USBD_ErrLog("Invalid Class handle");
#endif  /* USBD_DEBUG_LEVEL */
    return USBD_FAIL;
  }

  /* link the class to the USB Device handle */
  pdev->pClass[0] = pclass;
  /* get device configuration descriptor */
#ifdef  USB_OTG_HS_CORE
  if (pdev->pClass[pdev->classId]->GetHSConfigDescriptor != NULL)
  {
    pdev->pConfDesc = (void *)pdev->pClass[pdev->classId]->GetHSConfigDescriptor(&len);
  }
#elif USB_OTG_FS_CORE /* default USE_USB_FS */
  if (pdev->pClass[pdev->classId]->GetFSConfigDescriptor != NULL)
  {
    pdev->pConfDesc = (void *)pdev->pClass[pdev->classId]->GetFSConfigDescriptor(&len);
  }
#else /* USB_OTG_FS_CORE */
  {

  }
#endif  /* USB_OTG_HS_CORE */

  /* Increment the NumClasses */
  pdev->NumClasses++;

  return USBD_OK;
}

#ifdef  USE_USBD_COMPOSITE
/**
 * @brief  USBD_RegisterClassComposite
 *         Link class driver to device core.
 * @param  pdev: device handle
 * @param  pclass: class handle
 * @param  classtype: class type
 * @param  EpAddr: Endpoint address handle
 * @retval USBD Status
 */
USBD_StatusTypeDef  USBD_RegisterClassComposite(USBD_HandleTypeDef *pdev, USBD_ClassTypeDef *pclass,
                                                USBD_CompositeClassTypeDef classtype, uint8_t *EpAddr)
{
  USBD_StatusTypeDef ret = USBD_FAIL;
  uint16_t  len = 0U;

  /* Reject every invalid input up front so a failed registration is never
   * reported as success. NULL pdev/pclass/EpAddr or either capacity bound
   * being exceeded must return FAIL without mutating any state. */
  if ((pdev == NULL) || (pclass == NULL) || (EpAddr == NULL))
  {
#if (USBD_DEBUG_LEVEL > 1U)
    USBD_ErrLog("Invalid registration argument");
#endif
    return ret;
  }

  if ((pdev->classId < USBD_MAX_SUPPORTED_CLASS) && (pdev->NumClasses < USBD_MAX_SUPPORTED_CLASS))
  {
    /* Link the class to the usb device handle */
    pdev->pClass[pdev->classId] = pclass;
    pdev->tclasslist[pdev->classId].EpAdd = EpAddr;

    /* call the composite class builder; a failure (slot already active,
     * class type disabled/unsupported, or descriptor build failure) must
     * abort registration instead of leaving a partial-active slot. */
    if (USBD_CMPSIT_AddClass(pdev, pclass, classtype) != (uint8_t)USBD_OK)
    {
      pdev->pClass[pdev->classId] = NULL;
      pdev->tclasslist[pdev->classId].EpAdd = NULL;
      ret = USBD_FAIL;
    }
    else
    {
      ret = USBD_OK;

      /* Increment the ClassId for the next occurence */
      pdev->classId ++;
      pdev->NumClasses ++;
    }
  }
  /* capacity exceeded: ret stays USBD_FAIL (no body, no mutation) */

  if (ret == USBD_OK)
  {
    /* get device configuration descriptor */
#ifdef USB_OTG_HS_CORE
    pdev->pConfDesc = USBD_CMPSIT.GetHSConfigDescriptor(&len);
#elif USB_OTG_FS_CORE /* default USE_USB_FS */
    pdev->pConfDesc = USBD_CMPSIT.GetFSConfigDescriptor(&len);
#else /* USB_OTG_FS_CORE*/
    {}
#endif  /* USB_OTG_HS_CORE */
  }
  return ret;
}

/**
 * @brief  USBD_UnRegisterClassComposite
 *         Unlink all composite class drivers from device core.
 * @param  pdev: device handle
 * @retval USBD Status
 */
USBD_StatusTypeDef  USBD_UnRegisterClassComposite(USBD_HandleTypeDef *pdev)
{
  USBD_StatusTypeDef ret = USBD_FAIL;
  uint8_t  idx1;
  uint8_t  idx2;

  /* unroll all activated classes */
  for (idx1 = 0; idx1 < pdev->NumClasses; idx1++)
  {
    /* check if the class correspond to the requested type and if it is active */
    if (pdev->tclasslist[idx1].Active == 1U)
    {
      /* set the new class ID */
      pdev->classId = idx1;
      /* free resources used by the selected class */
      if (pdev->pClass[pdev->classId] != NULL)
      {
        /* free class resources */
        if (pdev->pClass[pdev->classId]->DeInit(pdev) != 0U)
        {
#if (USBD_DEBUG_LEVEL > 1U)
          USBD_ErrLog("Class DeInit didn't succeed!, can't unregister selected class");
#endif
          ret = USBD_FAIL;
        }
      }
      /* free the class pointer */
      pdev->pClass[pdev->classId] = NULL;
      /* free the class location in classes table and reset its parameters to zero */
      pdev->tclasslist[pdev->classId].ClassType   = CLASS_TYPE_NONE;
      pdev->tclasslist[pdev->classId].ClassId     = 0U;
      pdev->tclasslist[pdev->classId].Active      = 0U;
      pdev->tclasslist[pdev->classId].NumEps      = 0U;
      pdev->tclasslist[pdev->classId].NumIf       = 0U;
      pdev->tclasslist[pdev->classId].CurrPcktSze = 0U;

      for (idx2 = 0U; idx2 < USBD_MAX_CLASS_ENDPOINTS; idx2++)
      {
        pdev->tclasslist[pdev->classId].Eps[idx2].add     = 0U;
        pdev->tclasslist[pdev->classId].Eps[idx2].type    = 0U;
        pdev->tclasslist[pdev->classId].Eps[idx2].size    = 0U;
        pdev->tclasslist[pdev->classId].Eps[idx2].is_used = 0U;
      }
      for (idx2 = 0U; idx2 < USBD_MAX_CLASS_INTERFACES; idx2++)
      {
        pdev->tclasslist[pdev->classId].Ifs[idx2] = 0U;
      }
    }
  }

  /* reset the configuration descriptor */
  (void)USBD_CMPSIT_ClearConfDesc();

  /* reset the class ID and number of classes */
  pdev->classId = 0U;
  pdev->NumClasses = 0U;

  return ret;
}
#endif /* USE_USBD_COMPOSITE */

/**
 * @brief  USBD_Start
 *         Start the USB Device Core.
 * @param  pdev: device handle
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_Start(USBD_HandleTypeDef *pdev)
{
#ifdef USE_USBD_COMPOSITE
  pdev->classId = 0U;
#endif /* USE_USBD_COMPOSITE */

  /* strat the low level driver */
  return USBD_LL_Start(pdev);
}

/**
 * @brief  USBD_Stop
 *         Stop the USB Device Core.
 * @param  pdev: device handle
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_Stop(USBD_HandleTypeDef *pdev)
{
  /* disconnect USB device */
  (void)USBD_LL_Stop(pdev);
  /* free class resources */
#ifdef  USE_USBD_COMPOSITE
  /* parse the table of classes in use */
  for (uint32_t i = 0; i < USBD_MAX_SUPPORTED_CLASS; i++)
  {
    /* check if current class is in use */
    if ((pdev->tclasslist[i].Active) == 1U)
    {
      if (pdev->pClass[i] != NULL)
      {
        pdev->classId = i;
        /* free class resources */
        (void)pdev->pClass[i]->DeInit(pdev);
      }
    }
  }
  /* reset the class ID */
  pdev->classId = 0U;
#else
  /* free class resources */
  if (pdev->pClass[0] != NULL)
  {
    (void)pdev->pClass[0]->DeInit(pdev);
  }
#endif /* USE_USBD_COMPOSITE */

  return USBD_OK;
}

/**
 * @brief  USBD_RunTestMode
 *         only for USB_OTG_HS_CORE
 *         Launch test mode process.
 * @param  pdev: device handle
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_RunTestMode(USBD_HandleTypeDef *pdev)
{
#ifdef USBD_HS_TESTMODE_ENABLE
  USBD_StatusTypeDef ret;
  /* run usb hs test mode */
  ret = USBD_LL_SetTestMode(pdev, pdev->dev_test_mode);

  return ret;
#else
  /* prevent unused argument compilation warning */
  UNUSED(pdev);
  return USBD_OK;
#endif  /* USBD_HS_TESTMODE_ENABLE */
}

/**
 * @brief  USBD_SetClassConfig
 *         Configure device and start the interface.
 * @param  pdev: device instance
 * @param  cfgidx: configuration index
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_SetClassConfig(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  USBD_StatusTypeDef ret = USBD_OK;
  UNUSED(cfgidx);  /* Parameter not used in this implementation */

#ifdef  USE_USBD_COMPOSITE
  /* parse the table of classes in use */
  for (uint32_t i = 0; i < USBD_MAX_SUPPORTED_CLASS; i++)
  {
    /* check if current class is in use */
    if ((pdev->tclasslist[i].Active) == 1U)
    {
      if (pdev->pClass[i] != NULL)
      {
        pdev->classId = i;
        /* set configuration and start the class */
        if (pdev->pClass[i]->Init(pdev) != 0U)
        {
          ret = USBD_FAIL;
        }
      }
    }
  }
#else
  if (pdev->pClass[0] != NULL)
  {
    /* set configuration and start the class */
    ret = (USBD_StatusTypeDef)pdev->pClass[0]->Init(pdev);
  }
#endif /* USE_USBD_COMPOSITE */
  return ret;

}

/**
 * @brief  USBD_ClrClassConfig
 *         clear current configuration.
 * @param  pdev: device instance
 * @param  cfgidx: configuration index
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_ClrClassConfig(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
  USBD_StatusTypeDef ret = USBD_OK;
  UNUSED(cfgidx);  /* Parameter not used in this implementation */

#ifdef  USE_USBD_COMPOSITE
  /* parse the table of classes in use */
  for (uint32_t i = 0; i < USBD_MAX_SUPPORTED_CLASS; i++)
  {
    /* check if current class is in use */
    if ((pdev->tclasslist[i].Active) == 1U)
    {
      if (pdev->pClass[i] != NULL)
      {
        pdev->classId = i;
        /* clear configuration and De-initialize the class process */
        if (pdev->pClass[i]->DeInit(pdev) != 0U)
        {
          ret = USBD_FAIL;
        }
      }
    }
  }
#else
  /* clear configuration and De-initialize the class process */
  if (pdev->pClass[0]->DeInit(pdev) != 0U)
  {
    ret = USBD_FAIL;
  }
#endif /* USE_USBD_COMPOSITE */
  return ret;

}

/**
 * @brief  USBD_LL_SetupStage
 *         Handle the setup stage.
 * @param  pdev: device instance
 * @param  psetup: setup packet buffer pointer
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_SetupStage(USBD_HandleTypeDef *pdev, uint8_t *psetup)
{
  USBD_StatusTypeDef ret;

  USBD_ParseSetupRequest(&pdev->request, psetup);
  pdev->ep0_state = USBD_EP0_SETUP;

  pdev->ep0_data_len = pdev->request.wLength;

  switch (pdev->request.bmRequest & 0x1FU)
  {
    case USB_REQ_RECIPIENT_DEVICE:
      ret = USBD_StdDevReq(pdev, &pdev->request);
      break;

    case USB_REQ_RECIPIENT_INTERFACE:
      ret = USBD_StdItfReq(pdev, &pdev->request);
      break;

    case USB_REQ_RECIPIENT_ENDPOINT:
      ret = USBD_StdEPReq(pdev, &pdev->request);
      break;

    default:
      ret = USBD_LL_StallEP(pdev, (pdev->request.bmRequest & 0x80U));
      break;
  }
  return ret;
}

/**
 * @brief  USBD_LL_DataOutStage
 *         Handle data out stage.
 * @param  pdev: device instance
 * @param  epnum: endpoint index
 * @param  pdata: data pointer
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_DataOutStage(USBD_HandleTypeDef *pdev, uint8_t epnum, uint8_t *pdata)
{
  USBD_EndpointTypeDef *pep;
  USBD_StatusTypeDef ret = USBD_OK;
  uint8_t idx;

  if (epnum == 0U)
  {
    pep = &pdev->ep_out[0];

    if (pdev->ep0_state == USBD_EP0_DATA_OUT)
    {
      if (pep->rem_length > pep->maxpacket)
      {
        pep->rem_length -= pep->maxpacket;
        (void)USBD_CtlContinueRx(pdev, pdata, MIN(pep->rem_length, pep->maxpacket));
      }
      else
      {
        /* find the class ID relative to the current request */
        switch (pdev->request.bmRequest & 0x1FU)
        {
          case USB_REQ_RECIPIENT_DEVICE:
            /* device request must be management by the first instantiated class
             * (or duplicated by all classer for simplicity)*/
            idx = 0U;
            break;

          case USB_REQ_RECIPIENT_INTERFACE:
            idx = USBD_CoreFindIF(pdev, LOBYTE(pdev->request.wIndex));
            break;

          case USB_REQ_RECIPIENT_ENDPOINT:
            idx = USBD_CoreFindEP(pdev, LOBYTE(pdev->request.wIndex));
            break;

          default:
            /* back to the first class in case of doubt */
            idx = 0U;
            break;
        }
        if ((idx != 0xFFU) && (idx < USBD_MAX_SUPPORTED_CLASS))
        {
          /* setup the class ID and route the request to the relative class function */
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            if (pdev->pClass[idx] != NULL)
            {
              if (pdev->pClass[idx]->EP0_RxReady != NULL)
              {
                pdev->classId = idx;
                pdev->pClass[idx]->EP0_RxReady(pdev);
              }
            }
          }
        }
      #ifdef USB_OTG_HS_CORE
        (void)USBD_CtlSendStatus(pdev);
      #endif /* USB_OTG_HS_CORE */
      }
    }
  }
  else
  {
    /* get the class index relative to this interface */
    idx = USBD_CoreFindEP(pdev, (epnum & 0x7FU));

    if (((uint16_t)idx != 0xFFU) && (idx < USBD_MAX_SUPPORTED_CLASS))
    {
      /* call the class data out function to manage the request */
      if (pdev->dev_state == USBD_STATE_CONFIGURED)
      {
        if (pdev->pClass[idx]->DataOut != NULL)
        {
          pdev->classId = idx;
          ret = (USBD_StatusTypeDef)pdev->pClass[idx]->DataOut(pdev, epnum);
        }
      }
      if (ret != USBD_OK)
      {
        return ret;
      }
    }
  }
  return USBD_OK;
}

/**
 * @brief  USBD_LL_DataInStage
 *         Handle data in stage.
 * @param  pdev: device instance
 * @param  epnum: endpoint index
 * @param  pdata: data pointer
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_DataInStage(USBD_HandleTypeDef *pdev, uint8_t epnum, uint8_t *pdata)
{
  USBD_EndpointTypeDef *pep;
  USBD_StatusTypeDef  ret;
  uint8_t idx;

  if (epnum == 0U)
  {
    pep = &pdev->ep_in[0];

    if (pdev->ep0_state == USBD_EP0_DATA_IN)
    {
      if (pep->rem_length > pep->maxpacket)
      {
        pep->rem_length -= pep->maxpacket;
        (void)USBD_CtlContinueSendData(pdev, pdata, pep->rem_length);
        /* prepare endpoint for premature end of transfer */
        (void)USBD_LL_PrepareReceive(pdev, 0U, NULL, 0U);
      }
      else
      {
        /* last packet is MPS multiple, so send ZLP packet */
        if ((pep->maxpacket == pep->rem_length) && (pep->total_length >= pep->maxpacket) &&
            (pep->total_length < pdev->ep0_data_len))
        {
          (void)USBD_CtlContinueSendData(pdev, NULL, 0U);
          pdev->ep0_data_len = 0U;

          /* prepare endpoint for premature end of transfer */
          (void)USBD_LL_PrepareReceive(pdev, 0U, NULL, 0U);
        }
        else
        {
          if (pdev->dev_state == USBD_STATE_CONFIGURED)
          {
            if (pdev->pClass[0]->EP0_TxSent != NULL)
            {
              pdev->classId = 0U;
              pdev->pClass[0]->EP0_TxSent(pdev);
            }
          }
        #ifndef USB_OTG_FS_CORE
          (void)USBD_LL_StallEP(pdev, 0x80U);
        #endif /* USB_OTG_FS_CORE*/
          (void)USBD_CtlReceiveStatus(pdev);
        }
      }
    }
    if (pdev->dev_test_mode != 0U)
    {
      (void)USBD_RunTestMode(pdev);
      pdev->dev_test_mode = 0U;
    }
  }
  else
  {
    /* get the class index relative to this interface */
    idx = USBD_CoreFindEP(pdev, ((uint8_t)epnum | 0x80U));

    if (((uint16_t)idx != 0xFFU) && (idx < USBD_MAX_SUPPORTED_CLASS))
    {
      /* call the class data out function to manage the request */
      if (pdev->dev_state == USBD_STATE_CONFIGURED)
      {
        if (pdev->pClass[idx]->DataIn != NULL)
        {
          pdev->classId = idx;
          ret = (USBD_StatusTypeDef)pdev->pClass[idx]->DataIn(pdev, epnum);

          if (ret != USBD_OK)
          {
            return ret;
          }
        }
      }
    }
  }
  return USBD_OK;
}

/**
 * @brief  USBD_LL_Reset
 *         Handle reset event.
 * @param  pdev: device instance
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_Reset(USBD_HandleTypeDef *pdev)
{
  USBD_StatusTypeDef ret = USBD_OK;

  /* upon reset call user call back */
  pdev->dev_state = USBD_STATE_DEFAULT;
  pdev->ep0_state = USBD_EP0_IDLE;
  pdev->dev_config = 0U;
  pdev->dev_remote_wakeup = 0U;
  pdev->dev_test_mode = 0U;
  pdev->dev_address = 0xFF;

#ifdef  USE_USBD_COMPOSITE
  /* parse the table of classes in use */
  for (uint32_t i = 0; i < USBD_MAX_SUPPORTED_CLASS; i++)
  {
    /* check if current class is in use */
    if ((pdev->tclasslist[i].Active) == 1U)
    {
      if (pdev->pClass[i] != NULL)
      {
        pdev->classId = i;
        /* clear configuration and De-initialize the class process */
        if (pdev->pClass[i]->DeInit != NULL)
        {
          if (pdev->pClass[i]->DeInit(pdev) != USBD_OK)
          {
            ret = USBD_FAIL;
          }
        }
      }
    }
  }
#else
  if (pdev->pClass[0] != NULL)
  {
    if (pdev->pClass[0]->DeInit != NULL)
    {
      if (pdev->pClass[0]->DeInit(pdev) != USBD_OK)
      {
        ret = USBD_FAIL;
      }
    }
  }
#endif /* USE_USBD_COMPOSITE */

  /* open ep0 out */
  (void)USBD_LL_OpenEP(pdev, 0x00U, USBD_EP_TYPE_CTRL, USB_MAX_EP0_SIZE);
  pdev->ep_out[0x00U & 0xFU].is_used = 1U;

  pdev->ep_out[0].maxpacket = USB_MAX_EP0_SIZE;

  /* open ep0 in */
  (void)USBD_LL_OpenEP(pdev, 0x80U, USBD_EP_TYPE_CTRL, USB_MAX_EP0_SIZE);
  pdev->ep_in[0x80U & 0xFU].is_used = 1U;

  pdev->ep_in[0].maxpacket = USB_MAX_EP0_SIZE;

  return ret;
}

/**
 * @brief  USBD_LL_SetSpeed
 *         Set device speed.
 * @param  pdev: device instance
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_SetSpeed(USBD_HandleTypeDef *pdev, USBD_SpeedTypeDef speed)
{
  pdev->dev_speed = speed;
  return USBD_OK;
}

/**
 * @brief  USBD_LL_Suspend
 *         Handle suspend event.
 * @param  pdev: device instance
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_Suspend(USBD_HandleTypeDef *pdev)
{
  if (pdev->dev_state != USBD_STATE_SUSPENDED)
  {
    pdev->dev_old_state = pdev->dev_state;
  }
  pdev->dev_state = USBD_STATE_SUSPENDED;
  return USBD_OK;
}

/**
 * @brief  USBD_LL_Resume
 *         Handle resume event.
 * @param  pdev: device instance
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_Resume(USBD_HandleTypeDef *pdev)
{
  if (pdev->dev_state == USBD_STATE_SUSPENDED)
  {
    pdev->dev_state = pdev->dev_old_state;
  }
  return USBD_OK;
}

/**
 * @brief  USBD_LL_SOF
 *         Handle SOF event.
 * @param  pdev: device instance
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_SOF(USBD_HandleTypeDef *pdev)
{
  /* the sof event can be distributed for all classes that support it */
  if (pdev->dev_state == USBD_STATE_CONFIGURED)
  {
#ifdef  USE_USBD_COMPOSITE
    /* parse the table of classes in use */
    for (uint32_t i = 0; i < USBD_MAX_SUPPORTED_CLASS; i++)
    {
      /* check if current class is in use */
      if ((pdev->tclasslist[i].Active) == 1U)
      {
        if (pdev->pClass[i] != NULL)
        {
          if (pdev->pClass[i]->SOF != NULL)
          {
            pdev->classId = i;
            (void)pdev->pClass[i]->SOF(pdev);
          }
        }
      }
    }
#else
    if (pdev->pClass[0] != NULL)
    {
      if (pdev->pClass[0]->SOF != NULL)
      {
        (void)pdev->pClass[0]->SOF(pdev);
      }
    }
#endif /* USE_USBD_COMPOSITE */
  }
  return USBD_OK;
}

/**
 * @brief  USBD_LL_IsoINIncomplete
 *         Handle iso in incomplete event.
 * @param  pdev: device instance
 * @param  epnum: Endpoint number
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_IsoINIncomplete(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  if (pdev->pClass[pdev->classId] == NULL)
  {
    return USBD_FAIL;
  }
  if (pdev->dev_state == USBD_STATE_CONFIGURED)
  {
    if (pdev->pClass[pdev->classId]->IsoINIncomplete != NULL)
    {
      (void)pdev->pClass[pdev->classId]->IsoINIncomplete(pdev, epnum);
    }
  }
  return USBD_OK;
}

/**
 * @brief  USBD_LL_IsoOUTIncomplete
 *         Handle iso out incomplete event.
 * @param  pdev: device instance
 * @param  epnum: Endpoint number
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_IsoOUTIncomplete(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
  if (pdev->pClass[pdev->classId] == NULL)
  {
    return USBD_FAIL;
  }
  if (pdev->dev_state == USBD_STATE_CONFIGURED)
  {
    if (pdev->pClass[pdev->classId]->IsoOUTIncomplete != NULL)
    {
      (void)pdev->pClass[pdev->classId]->IsoOUTIncomplete(pdev, epnum);
    }
  }
  return USBD_OK;
}

/**
 * @brief  USBD_LL_DevConnected
 *         Handle device connection event.
 * @param  pdev: device instance
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_DevConnected(USBD_HandleTypeDef *pdev)
{
  /* prevent unused argument compilation warning */
  UNUSED(pdev);
  return USBD_OK;
}

/**
 * @brief  USBD_LL_DevDisconnected
 *         Handle device disconnection event.
 * @param  pdev: device instance
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_DevDisconnected(USBD_HandleTypeDef *pdev)
{
  USBD_StatusTypeDef  ret = USBD_OK;

  /* free class resources */
  pdev->dev_state = USBD_STATE_DEFAULT;

#ifdef  USE_USBD_COMPOSITE
    /* parse the table of classes in use */
    for (uint32_t i = 0; i < USBD_MAX_SUPPORTED_CLASS; i++)
    {
      /* check if current class is in use */
      if ((pdev->tclasslist[i].Active) == 1U)
      {
        if (pdev->pClass[i] != NULL)
        {
          pdev->classId = i;
          /* clear configuration and De-initialize the class process */
          if (pdev->pClass[i]->DeInit(pdev) != 0U)
          {
            ret = USBD_FAIL;
          }
        }
      }
    }
#else
    if (pdev->pClass[0] != NULL)
    {
      if (pdev->pClass[0]->DeInit(pdev) != 0U)
      {
        ret = USBD_FAIL;
      }
    }
#endif /* USE_USBD_COMPOSITE */

  return ret;
}

/**
 * @brief  USBD_CoreFindIF
 *         Return the class index relative to the selected interface
 * @param  pdev: device instance
 * @param  index: selected interface number
 * @retval index of the class using the selected interface number. 0xFF if no class found
 */
uint8_t USBD_CoreFindIF(USBD_HandleTypeDef *pdev, uint8_t index)
{
#ifdef  USE_USBD_COMPOSITE
  /* parse the table of classes in use */
  for (uint32_t i = 0; i < USBD_MAX_SUPPORTED_CLASS; i++)
  {
    /* check if current class is in use */
    if ((pdev->tclasslist[i].Active) == 1U)
    {
      /* parse all interfaces listed in the current class */
      for (uint32_t j = 0U; j < pdev->tclasslist[i].NumIf; j++)
      {
        /* check if requested interface matches the current class interface */
        if (pdev->tclasslist[i].Ifs[j] == index)
        {
          if (pdev->pClass[i]->Setup != NULL)
          {
            return (uint8_t)i;
          }
        }
      }
    }
  }
  return  0xFFU;
#else
  UNUSED(pdev);
  UNUSED(index);

  return 0x00U;
#endif /* USE_USBD_COMPOSITE */
}

/**
 * @brief  USBD_CoreFindEP
 *         Return the class index relative to the selected endpoint
 * @param  pdev: device instance
 * @param  index: selected endpoint number
 * @retval index of the class using the selected endpoint number. 0xFF if no class found
 */
uint8_t USBD_CoreFindEP(USBD_HandleTypeDef *pdev, uint8_t index)
{
#ifdef  USE_USBD_COMPOSITE
  /* parse the table of classes in use */
  for (uint32_t i = 0; i < USBD_MAX_SUPPORTED_CLASS; i++)
  {
    /* check if current class is in use */
    if ((pdev->tclasslist[i].Active) == 1U)
    {
      /* parse all endpoints listed in the current class */
      for (uint32_t j = 0U; j < pdev->tclasslist[i].NumEps; j++)
      {
        /* check if requested endpoint matches the current class endpoint */
        if (pdev->tclasslist[i].Eps[j].add == index)
        {
          if (pdev->pClass[i]->Setup != NULL)
          {
            return (uint8_t)i;
          }
        }
      }
    }
  }
  return  0xFFU;
#else
  UNUSED(pdev);
  UNUSED(index);

  return 0x00U;
#endif /* USE_USBD_COMPOSITE */
}

#ifdef  USE_USBD_COMPOSITE
/**
 * @brief  USBD_CoreGetEPAdd
 *         Get the endpoint address relative to a selected class
 * @param  pdev: device instance
 * @param  ep_dir: USBD_EP_IN or USBD_EP_OUT
 * @param  ep_type: USBD_EP_TYPE_CTRL, USBD_EP_TYPE_ISOC, USBD_EP_TYPE_BULK or USBD_EP_TYPE_INTR
 * @param  ClassId: class ID
 * @retval Address of the selected endpoint or 0xFFU if no endpoint found
 */
uint8_t USBD_CoreGetEPAdd(USBD_HandleTypeDef *pdev, uint8_t ep_dir, uint8_t ep_type, uint8_t ClassId)
{
  uint8_t idx;
  /* Find the EP address in the selected class table */
  for (idx = 0; idx < pdev->tclasslist[ClassId].NumEps; idx++)
  {
    if (((pdev->tclasslist[ClassId].Eps[idx].add & USBD_EP_IN) == ep_dir) &&
         (pdev->tclasslist[ClassId].Eps[idx].type == ep_type) &&
         (pdev->tclasslist[ClassId].Eps[idx].is_used != 0U))
    {
      return (pdev->tclasslist[ClassId].Eps[idx].add);
    }
  }
  /* if reaching this point, then no endpoint was found */
  return 0xFFU;
}
#endif /* USE_USBD_COMPOSITE */

/**
 * @brief  USBD_GetEpDesc
 *         this function return the endpoint descriptor.
 * @param  pConfDesc: pointer to bos descriptor
 * @param  EpAddr: endpoint address
 * @retval pointer to video endpoint descriptor
 */
void *USBD_GetEpDesc(uint8_t *pConfDesc, uint8_t EpAddr)
{
  USBD_DescHeaderTypeDef *pdesc = (USBD_DescHeaderTypeDef *)(void *)pConfDesc;
  USBD_ConfigDescTypeDef *desc  = (USBD_ConfigDescTypeDef *)(void *)pConfDesc;
  USBD_EpDescTypeDef *pEpDesc = NULL;
  uint16_t ptr;

  if (desc->wTotalLength > desc->bLength)
  {
    ptr = desc->bLength;
    while (ptr < desc->wTotalLength)
    {
      pdesc = USBD_GetNextDesc((uint8_t *)pdesc, &ptr);
      if (pdesc->bDescriptorType == USB_DESC_TYPE_ENDPOINT)
      {
        pEpDesc = (USBD_EpDescTypeDef *)(void *)pdesc;
        if (pEpDesc->bEndpointAddress == EpAddr)
        {
          break;
        }
        else
        {
          pEpDesc = NULL;
        }
      }
    }
  }
  return  (void *)pEpDesc;

}

/**
 * @brief  USBD_GetNextDesc
 *         this function return the next descriptor header.
 * @param  pbuf: buffer where the descriptor is available.
 * @param  ptr: data pointer inside the descriptor
 * @retval next header
 */
USBD_DescHeaderTypeDef *USBD_GetNextDesc(uint8_t *pbuf, uint16_t *ptr)
{
  USBD_DescHeaderTypeDef *pnext = (USBD_DescHeaderTypeDef *)(void *)pbuf;

  *ptr += pnext->bLength;
  pnext = (USBD_DescHeaderTypeDef *)(void *)(pbuf + pnext->bLength);

  return (pnext);
}

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
