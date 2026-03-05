from mythic_container.MythicCommandBase import *
from mythic_container.MythicRPC import *


class JobKillArguments(TaskArguments):
    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = []

    async def parse_arguments(self):
        if not self.command_line:
            raise Exception("Require a task ID to kill as a command line argument.")
        self.add_arg("task_id", self.command_line.strip())

    async def parse_dictionary(self, dictionary_arguments):
        self.load_args_from_dictionary(dictionary_arguments)


class JobKillCommand(CommandBase):
    cmd = "job_kill"
    needs_admin = False
    help_cmd = "job_kill [task-uuid]"
    description = (
        "Cancel a queued or in-progress upload task (including lateral movement file transfers). "
        "Searches the internal upload queue for the given task UUID, cancels the transfer, "
        "closes the partial file, and reports the task as failed in Mythic."
    )
    version = 1
    is_exit = False
    supported_ui_features = ["jobkill", "task:job_kill"]
    author = "@Lavender-exe"
    attackmapping = []
    argument_class = JobKillArguments
    attributes = CommandAttributes(
        builtin=False,
        supported_os=[SupportedOS.Windows],
        suggested_command=False,
    )

    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(
            TaskID=taskData.Task.ID,
            Success=True,
        )
        killed_task_resp = await SendMythicRPCTaskSearch(MythicRPCTaskSearchMessage(
            TaskID=taskData.Task.ID,
            SearchAgentTaskID=taskData.args.command_line,
        ))
        if killed_task_resp.Success and len(killed_task_resp.Tasks) > 0:
            task = killed_task_resp.Tasks[0]
            response.DisplayParams = f"{task.CommandName} {task.DisplayParams}"
        else:
            response.DisplayParams = taskData.args.command_line
        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp
