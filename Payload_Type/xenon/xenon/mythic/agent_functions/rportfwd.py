from mythic_container.MythicCommandBase import *
import json
from mythic_container.MythicRPC import *

class RportfwdArguments(TaskArguments):

    def __init__(self, command_line, **kwargs):
        super().__init__(command_line, **kwargs)
        self.args = [
            CommandParameter(
                name="action",
                cli_name="Action",
                display_name="Action",
                type=ParameterType.ChooseOne,
                choices=["start", "stop"],
                default_value="start",
                description="Start or stop reverse port forward on the target host.",
                parameter_group_info=[ParameterGroupInfo(
                    ui_position=0,
                    required=False
                )],
            ),
            CommandParameter(
                name="port",
                cli_name="Port",
                display_name="Port",
                type=ParameterType.Number,
                description="Port to listen for connections on the target host.",
                parameter_group_info=[ParameterGroupInfo(
                    ui_position=1,
                    required=True
                )]
            ),
            CommandParameter(
                name="remote_ip",
                cli_name="RemoteIP",
                display_name="Remote IP",
                type=ParameterType.String,
                description="Remote IP to forward rportfwd traffic to.",
                parameter_group_info=[ParameterGroupInfo(
                    required=True,
                    ui_position=2,
                )]
            ),
            CommandParameter(
                name="remote_port",
                cli_name="RemotePort",
                display_name="Remote Port",
                type=ParameterType.Number,
                description="Remote port to forward rportfwd traffic to.",
                parameter_group_info=[ParameterGroupInfo(
                    required=True,
                    ui_position=3,
                )]
            ),
            CommandParameter(
                name="username",
                cli_name="Username",
                display_name="Port Auth Username",
                type=ParameterType.String,
                description="Must auth as this user to use the port.",
                parameter_group_info=[ParameterGroupInfo(
                    required=False,
                    ui_position=4,
                )]
            ),
            CommandParameter(
                name="password",
                cli_name="Password",
                display_name="Port Auth Password",
                type=ParameterType.String,
                description="Must auth with this password to use the port.",
                parameter_group_info=[ParameterGroupInfo(
                    required=False,
                    ui_position=5,
                )]
            ),
        ]

    async def parse_arguments(self):
        if len(self.command_line) == 0:
            raise Exception("Must be passed a port on the command line.")
        try:
            self.load_args_from_json_string(self.command_line)
        except:
            parts = self.command_line.strip().split()
            if len(parts) < 3:
                raise Exception("Invalid command line. Expected: rportfwd [port] [remote_ip] [remote_port]")
            try:
                self.add_arg("port", int(parts[0]))
                self.add_arg("remote_ip", parts[1])
                self.add_arg("remote_port", int(parts[2]))
                self.add_arg("action", "start")
            except Exception as e:
                raise Exception("Invalid rportfwd arguments: {}. Port and remote_port must be int.".format(self.command_line))


class RportfwdCommand(CommandBase):
    cmd = "rportfwd"
    needs_admin = False
    help_cmd = "rportfwd -Action {start|stop} -Port [port] -RemoteIP [ip] -RemotePort [port]"
    description = "Listen on a port on the target host and forward traffic through Mythic to remoteIP:remotePort."
    version = 1
    script_only = False
    author = "@djhohnstein"
    argument_class = RportfwdArguments
    attackmapping = ["T1090"]
    attributes=CommandAttributes(
        builtin=False,
        dependencies=[],
        suggested_command=False
    )
    async def create_go_tasking(self, taskData: PTTaskMessageAllData) -> PTTaskCreateTaskingMessageResponse:
        response = PTTaskCreateTaskingMessageResponse(
            TaskID=taskData.Task.ID,
            Success=True,
        )
        response.DisplayParams = (
            f"{taskData.args.get_arg('action')} {taskData.args.get_arg('port')} "
            f"{taskData.args.get_arg('remote_ip')} {taskData.args.get_arg('remote_port')}"
        )
        if taskData.args.get_arg('username') != "" and taskData.args.get_arg('username') is not None:
            response.DisplayParams += f" -Username {taskData.args.get_arg('username')} -Password {taskData.args.get_arg('password')}"

        if taskData.args.get_arg("action") == "start":
            resp = await SendMythicRPCProxyStartCommand(MythicRPCProxyStartMessage(
                TaskID=taskData.Task.ID,
                PortType="rpfwd",
                LocalPort=taskData.args.get_arg("port"),
                RemoteIP=taskData.args.get_arg("remote_ip"),
                RemotePort=taskData.args.get_arg("remote_port"),
                Username=taskData.args.get_arg("username"),
                Password=taskData.args.get_arg("password")
            ))
            if not resp.Success:
                response.TaskStatus = MythicStatus.Error
                response.Stderr = resp.Error
                await SendMythicRPCResponseCreate(MythicRPCResponseCreateMessage(
                    TaskID=taskData.Task.ID,
                    Response=resp.Error.encode()
                ))
            else:
                await SendMythicRPCResponseCreate(MythicRPCResponseCreateMessage(
                    TaskID=taskData.Task.ID,
                    Response=(
                        f"Starting reverse port forward on port {taskData.args.get_arg('port')} "
                        f"-> {taskData.args.get_arg('remote_ip')}:{taskData.args.get_arg('remote_port')}\n"
                        f"Updating Sleep to 0\n"
                    ).encode()
                ))
                await SendMythicRPCTaskCreateSubtask(MythicRPCTaskCreateSubtaskMessage(
                    TaskID=taskData.Task.ID,
                    CommandName="sleep",
                    Params=json.dumps({
                        "seconds": 0,
                    })
                ))
        else:
            resp = await SendMythicRPCProxyStopCommand(MythicRPCProxyStopMessage(
                TaskID=taskData.Task.ID,
                PortType="rpfwd",
                Port=taskData.args.get_arg("port"),
                Username=taskData.args.get_arg("username"),
                Password=taskData.args.get_arg("password")
            ))

            if not resp.Success:
                response.TaskStatus = MythicStatus.Error
                response.Stderr = resp.Error
                await SendMythicRPCResponseCreate(MythicRPCResponseCreateMessage(
                    TaskID=taskData.Task.ID,
                    Response=resp.Error.encode()
                ))
            else:
                response.TaskStatus = MythicStatus.Success
                await SendMythicRPCResponseCreate(MythicRPCResponseCreateMessage(
                    TaskID=taskData.Task.ID,
                    Response=f"Stopped reverse port forward on port {taskData.args.get_arg('port')}\nUpdating Sleep to 1\n".encode()
                ))
                await SendMythicRPCTaskCreateSubtask(MythicRPCTaskCreateSubtaskMessage(
                    TaskID=taskData.Task.ID,
                    CommandName="sleep",
                    Params=json.dumps({
                        "seconds": 1,
                    })
                ))

        return response

    async def process_response(self, task: PTTaskMessageAllData, response: any) -> PTTaskProcessResponseMessageResponse:
        resp = PTTaskProcessResponseMessageResponse(TaskID=task.Task.ID, Success=True)
        return resp
