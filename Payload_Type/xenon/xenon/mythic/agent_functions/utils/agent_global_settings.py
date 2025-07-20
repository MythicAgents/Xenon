
class ProcessInjectKit:
    ''' 
    Manage the custom BOF files operators can upload for process injection 
    '''
    def __init__(self, inject_spawn: str = "", inject_explicit: str = "", named_pipe_stub: bytes = b""):
        self._inject_spawn = inject_spawn
        self._inject_explicit = inject_explicit
        self._named_pipe_stub = named_pipe_stub

    # Getter for inject_spawn
    def get_inject_spawn(self) -> str:
        return self._inject_spawn

    # Setter for inject_spawn
    def set_inject_spawn(self, value: str):
        if not isinstance(value, str):
            raise TypeError("inject_spawn must be a string")
        self._inject_spawn = value

    # Getter for inject_explicit
    def get_inject_explicit(self) -> str:
        return self._inject_explicit

    # Setter for inject_explicit
    def set_inject_explicit(self, value: str):
        if not isinstance(value, str):
            raise TypeError("inject_explicit must be a string")
        self._inject_explicit = value


# Global
PROCESS_INJECT_KIT = ProcessInjectKit()


