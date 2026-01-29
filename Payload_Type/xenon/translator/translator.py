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
        
        # C2 -> Agent
        logging.info(f"[{mythic_action}] C2 -> Agent : {len(inputMsg.Message)} bytes")
        logging.info(f"[{mythic_action}] C2 -> Agent : {inputMsg.Message} \n\n")
        
        if mythic_action == "checkin":
            main_msg = checkin_to_agent_format(inputMsg.Message["id"])
        
        elif mythic_action == "get_tasking":
            main_msg = get_responses_to_agent_format(inputMsg)
        
        response.Message = main_msg
        
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
            mythic_type = "checkin response"
            response.Message = checkin_to_mythic_format(mythic_action_data)
        
        elif mythic_action_byte == MYTHIC_GET_TASKING:
            mythic_type = "get_tasking response"
            response.Message = post_response_handler(mythic_action_data)
        
        else:
            mythic_type = f"UNKNOWN_RESPONSE: {mythic_action_byte}"

        # Agent -> C2
        logging.info(f"[{mythic_type}] Agent -> C2 : {len(response.Message)} bytes")
        # logging.info(f"[{mythic_type}] Agent -> C2 : {response.Message} \n\n")
        
        return response
