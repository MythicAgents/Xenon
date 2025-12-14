import json
import base64
import binascii
import os
import logging

from translator.utils import *
from translator.commands_from_c2 import *
from translator.commands_from_implant import *
from mythic_container.TranslationBase import *


logging.basicConfig(level=logging.INFO)


class XenonTranslator(TranslationContainer):
    name = "XenonTranslator"
    description = "Translator for Xenon agent"
    author = "@c0rnbread"

    # #This doesn't get used since agent uses option mythic_encrypts=True 
    # async def generate_keys(self, inputMsg: TrGenerateEncryptionKeysMessage) -> TrGenerateEncryptionKeysMessageResponse:
    #     response = TrGenerateEncryptionKeysMessageResponse(Success=True)
    #     response.DecryptionKey = b""
    #     response.EncryptionKey = b""
    #     return response

    async def translate_to_c2_format(self, inputMsg: TrMythicC2ToCustomMessageFormatMessage) -> TrMythicC2ToCustomMessageFormatMessageResponse:
        """
        Handle messages coming from the C2 server destined for Agent.
        C2 --(this message)--> Agent
        """
        response = TrMythicC2ToCustomMessageFormatMessageResponse(Success=True)
        
        # Handle different Mythic message types
        mythic_action = inputMsg.Message["action"]
        
        if mythic_action == "checkin":
            main_msg = checkin_to_agent_format(inputMsg.Message["id"])
        
        elif mythic_action == "get_tasking":
            main_msg = get_tasking_to_agent_format(inputMsg.Message["tasks"])
        
        elif mythic_action == "post_response":
            main_msg = post_response_to_agent_format(inputMsg.Message["responses"])
        
        # Any delegate messages coming from Mythic get prepended
        delegates_msg = b""
        
        if mythic_action != "checkin":
            # logging.info(f"C2 --> Agent: {mythic_action} - {inputMsg.Message}")
            delegates_msg = delegates_to_agent_format(inputMsg)
    
    
        # logging.info(f"[{mythic_action}] Agent -> C2 : {delegates_msg + main_msg}")
        logging.info(f"[{mythic_action}] Agent -> C2 : {inputMsg.Message}")
        
        response.Message = delegates_msg + main_msg
        
        return response


    async def translate_from_c2_format(self, inputMsg: TrCustomMessageToMythicC2FormatMessage) -> TrCustomMessageToMythicC2FormatMessageResponse:
        """
        Handle messages coming from the Agent destined for C2.
        Agent --(this message)--> C2
        """
        response = TrCustomMessageToMythicC2FormatMessageResponse(Success=True)
        
        # Agent message (type + buffer)
        agent_action_msg = inputMsg.Message
        mythic_action_byte = agent_action_msg[0]
        mythic_action_data = agent_action_msg[1:]

        if mythic_action_byte == MYTHIC_CHECK_IN:
            response.Message = checkin_to_mythic_format(mythic_action_data)
        
        elif mythic_action_byte == MYTHIC_GET_TASKING: 
            response.Message = get_tasking_to_mythic_format(mythic_action_data)
        
        elif mythic_action_byte == MYTHIC_POST_RESPONSE: 
            response.Message = post_response_to_mythic_format(mythic_action_data)
        
        elif mythic_action_byte == MYTHIC_INIT_DOWNLOAD: 
            response.Message = download_init_to_mythic_format(mythic_action_data)
        
        elif mythic_action_byte == MYTHIC_CONT_DOWNLOAD: 
            response.Message = download_cont_to_mythic_format(mythic_action_data)
        
        elif mythic_action_byte == MYTHIC_UPLOAD_CHUNKED: 
            response.Message = upload_to_mythic_format(mythic_action_data)
            
        elif mythic_action_byte == MYTHIC_UPLOAD_CHUNKED: 
            response.Message = upload_to_mythic_format(mythic_action_data)
            
        elif mythic_action_byte == MYTHIC_P2P_CHECK_IN:
            response.Message = p2p_checkin_to_mythic_format(mythic_action_data)
            
        elif mythic_action_byte == MYTHIC_P2P_MSG:
            response.Message = p2p_to_mythic_format(mythic_action_data)

        return response
