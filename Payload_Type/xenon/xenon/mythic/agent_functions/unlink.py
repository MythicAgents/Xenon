from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *
import logging

class UnlinkArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="Id",
                display_name="Display Id",
                type=ParameterType.ChooseOne,
                dynamic_query_function=self.get_active_agent_ids,
                description="Display ID of the P2P Agent to disconnect from."
            )
        ]

    async def get_active_agent_ids(self, callback: PTRPCDynamicQueryFunctionMessage) -> PTRPCDynamicQueryFunctionMessageResponse:
        response = PTRPCDynamicQueryFunctionMessageResponse()
        try:
            # Get all active callbacks (agents) in the operation
            # Search for callbacks in the same operation, active only
            callback_resp = await SendMythicRPCCallbackEdgeSearch(MythicRPCCallbackEdgeSearchMessage(   
                AgentCallbackID=callback.Callback,
            ))

            if callback_resp.Success:
                agent_display_ids = []

                if hasattr(callback_resp, 'Results') and callback_resp.Results:
                    for result in callback_resp.Results:
                        # Check if Edge is different callback and is Alive
                        destination = result.Destination
                        if destination.get("agent_callback_id") != callback.AgentCallbackID and destination.get("dead") == False:
                            #agent = {destination.get("display_id"): destination.get("agent_callback_id")}
                            agent = destination.get("display_id")
                            agent_display_ids.append(str(agent))

                # Filter out duplicates and sort
                agent_display_ids = sorted(list(set(agent_display_ids)))
                
                logging.info(f"Found {len(agent_display_ids)} active linked agents: {agent_display_ids}")

                # Ensure Choices is always a list (even if empty)
                response.Success = True
                response.Choices = agent_display_ids

                return response
            else:
                await SendMythicRPCOperationEventLogCreate(MythicRPCOperationEventLogCreateMessage(
                    CallbackId=callback.Callback,
                    Message=f"Failed to get active agents: {callback_resp.Error}",
                    MessageLevel="warning"
                ))
                response.Success = False
                response.Error = f"Failed to get active agents: {callback_resp.Error}"
                response.Choices = []
                return response
            
        except Exception as e:
            logging.error(f"Error getting active agent IDs: {e}")
            import traceback
            logging.error(traceback.format_exc())
            response.Success = False
            response.Error = f"Error getting active agent IDs: {str(e)}"
            response.Choices = []
            return response

    async def parse_arguments(self):
        if len(self.command_line) == 0:
            raise ValueError("Must supply a command to run")
        self.add_arg("command", self.command_line)

    async def parse_dictionary(self, dictionary_arguments):
        self.load_args_from_dictionary(dictionary_arguments)

class UnlinkCommand(CommandBase):
    cmd = "unlink"
    needs_admin = False
    help_cmd = "unlink [Display Id]"      
    description = "Disconnect from an SMB Link Agent."
    version = 1
    author = "@c0rnbread"
    attackmapping = []
    argument_class = UnlinkArguments
    attributes = CommandAttributes(
        builtin=False,
        supported_os=[ SupportedOS.Windows ],
        suggested_command=False
    )

    # async def create_tasking(self, task: MythicTask) -> MythicTask:
    #     task.display_params = task.args.get_arg("command")
    #     return task
    
    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(
            TaskID=taskData.Task.ID,
            Success=True,
        )
        
        
        # Get the selected display ID
        selected_display_id = taskData.args.get_arg("Id")
        
        if not selected_display_id:
            response.Success = False
            response.Error = "No display ID provided"
            return response
        
        # Look up the agent callback ID from the display ID
        agent_callback_id = None
        try:
            callback_resp = await SendMythicRPCCallbackEdgeSearch(MythicRPCCallbackEdgeSearchMessage(   
                AgentCallbackID=taskData.Callback.ID,
            ))
            
            if callback_resp.Success and hasattr(callback_resp, 'Results') and callback_resp.Results:
                for result in callback_resp.Results:
                    destination = result.Destination
                    # Check if this destination matches the selected display_id
                    # Try attribute access first (if destination is an object)
                    if hasattr(destination, 'display_id') and hasattr(destination, 'agent_callback_id'):
                        if str(destination.display_id) == str(selected_display_id):
                            agent_callback_id = str(destination.agent_callback_id)
                            break
                    # Fallback: try using .get() if destination is dict-like
                    elif hasattr(destination, 'get'):
                        if str(destination.get("display_id")) == str(selected_display_id):
                            agent_callback_id = str(destination.get("agent_callback_id"))
                            break
            
            if not agent_callback_id:
                response.Success = False
                response.Error = f"Could not find agent callback ID for display ID: {selected_display_id}"
                return response
            
            # Set the agent callback ID as the argument to send to the agent
            # The agent expects this as "payload_id" based on the original parameter name
            taskData.args.set_arg("payload_id", agent_callback_id)
            
            # Remove the display ID argument so it doesn't get sent to the agent
            taskData.args.remove_arg("Id")
            
            # Set display parameters (show the display ID to the user)
            response.DisplayParams = "{}".format(selected_display_id)
            
        except Exception as e:
            logging.error(f"Error looking up agent callback ID: {e}")
            import traceback
            logging.error(traceback.format_exc())
            response.Success = False
            response.Error = f"Error looking up agent callback ID: {str(e)}"
            return response
        
        logging.info(f"Arguments: {taskData.args}")
        
        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp