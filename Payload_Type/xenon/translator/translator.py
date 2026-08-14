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

        Push C2 may deliver socks/rpfwd/delegates on get_tasking OR post_response
        (or other actions). Pack any message that carries agent work.
        """
        response = TrMythicC2ToCustomMessageFormatMessageResponse(Success=True)

        msg = inputMsg.Message if isinstance(inputMsg.Message, dict) else {}
        mythic_action = msg.get("action")

        socks = msg.get("socks") or []
        rpfwd = msg.get("rpfwd") or []
        tasks = msg.get("tasks") or []
        responses = msg.get("responses") or []
        delegates = msg.get("delegates") or []

        # logging.info(f"[{mythic_action}] C2 -> Agent : keys={list(msg.keys())}")
        # if socks:
        #     logging.info(f"[SOCKS] C2 -> Agent : {len(socks)} message(s)")
        # if rpfwd:
        #     logging.info(f"[RPORTFWD] C2 -> Agent : {len(rpfwd)} message(s)")

        try:
            if mythic_action == "checkin":
                main_msg = checkin_to_agent_format(msg["id"])

            elif (
                mythic_action in ("get_tasking", "post_response")
                or tasks
                or responses
                or delegates
                or socks
                or rpfwd
            ):
                # Pack tasks, responses, delegates, socks, rpfwd into agent GET_TASKING msg
                main_msg = get_responses_to_agent_format(inputMsg)

            else:
                response.Success = False
                response.Error = f"Unsupported Mythic action for Xenon translator: {mythic_action!r}"
                logging.error(response.Error)
                return response

            response.Message = main_msg

        except Exception as e:
            response.Success = False
            response.Error = f"translate_to_c2_format failed for action={mythic_action!r}: {e}"
            logging.exception(response.Error)

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
        # logging.info(f"[{mythic_type}] Agent -> C2 : {len(response.Message)} bytes")
        # logging.info(f"[{mythic_type}] Agent -> C2 : {response.Message} \n\n")
        
        return response
