import rospy
from std_srvs.srv import Trigger

from march_shared_resources.msg import CurrentGait, CurrentState, Error
from march_shared_resources.srv import (ContainsGait, ContainsGaitResponse, PossibleGaits, PossibleGaitsResponse,
                                        SetGaitVersion)

from .gait_selection import GaitSelection
from .state_machine.gait_state_machine import GaitStateMachine
from .state_machine.state_machine_input import StateMachineInput

NODE_NAME = 'gait_selection'
DEFAULT_GAIT_FILES_PACKAGE = 'march_gait_files'
DEFAULT_GAIT_DIRECTORY = 'minimal'
DEFAULT_UPDATE_RATE = 30.0


def set_gait_versions(msg, gait_selection):
    """Sets a new gait version to the gait selection instance.

    :type msg: march_shared_resources.srv.SetGaitVersionRequest
    :type gait_selection: GaitSelection
    :rtype march_shared_resources.srv.SetGaitVersionResponse
    """
    if len(msg.subgaits) != len(msg.versions):
        return [False, '`subgaits` and `versions` array are not of equal length']

    version_map = dict(zip(msg.subgaits, msg.versions))
    try:
        gait_selection.set_gait_versions(msg.gait, version_map)
        return [True, '']
    except Exception as e:
        return [False, str(e)]


def contains_gait(request, gait_selection):
    """
    Checks whether a gait and subgait are loaded.

    :type request: ContainsGaitRequest
    :param request: service request
    :param gait_selection: current loaded gaits
    :return: True when the gait and subgait are loaded
    """
    gait = gait_selection[request.gait]
    if gait is None:
        return ContainsGaitResponse(False)
    for subgait in request.subgaits:
        if gait[subgait] is None:
            return ContainsGaitResponse(False)

    return ContainsGaitResponse(True)


def error_cb(gait_state_machine, msg):
    if msg.type == Error.NON_FATAL:
        rospy.logerr('Stopping current gait. reason: {0}'.format(msg.error_message))
        gait_state_machine.stop()
    elif msg.type == Error.FATAL:
        rospy.logerr('Requesting shutdown. reason: {0}'.format(msg.error_message))
        gait_state_machine.request_shutdown()


def main():
    rospy.init_node(NODE_NAME)
    gait_package = rospy.get_param('~gait_package', DEFAULT_GAIT_FILES_PACKAGE)
    gait_directory = rospy.get_param('~gait_directory', DEFAULT_GAIT_DIRECTORY)
    update_rate = rospy.get_param('~update_rate', DEFAULT_UPDATE_RATE)

    gait_selection = GaitSelection(gait_package, gait_directory)
    rospy.loginfo('Gait selection initialized with package {0} of directory {1}'.format(gait_package, gait_directory))

    state_input = StateMachineInput()
    gait_state_machine = GaitStateMachine(gait_selection, state_input, update_rate)
    rospy.loginfo('Gait state machine successfully generated')

    rospy.core.add_preshutdown_hook(lambda reason: gait_state_machine.request_shutdown())

    # Services
    rospy.Service('/march/gait_selection/get_version_map', Trigger,
                  lambda msg: [True, str(gait_selection.gait_version_map)])

    rospy.Service('/march/gait_selection/set_gait_version', SetGaitVersion,
                  lambda msg: set_gait_versions(msg, gait_selection))

    rospy.Service('/march/gait_selection/get_directory_structure', Trigger,
                  lambda msg: [True, str(gait_selection.scan_directory())])

    rospy.Service('/march/gait_selection/update_default_versions', Trigger,
                  lambda msg: gait_selection.update_default_versions())

    rospy.Service('/march/gait_selection/contains_gait', ContainsGait,
                  lambda msg: contains_gait(msg, gait_selection))

    rospy.Service('/march/gait_selection/get_possible_gaits', PossibleGaits,
                  lambda msg: PossibleGaitsResponse(gaits=gait_state_machine.get_possible_gaits()))

    # Subscribers
    rospy.Subscriber('/march/error', Error, lambda msg: error_cb(gait_state_machine, msg))

    # Publishers
    current_state_pub = rospy.Publisher('/march/gait_selection/current_state', CurrentState, queue_size=10)
    current_gait_pub = rospy.Publisher('/march/gait_selection/current_gait', CurrentGait, queue_size=10)

    def current_state_cb(state, is_idle):
        current_state_pub.publish(state=state, state_type=CurrentState.IDLE if is_idle else CurrentState.GAIT)

    def current_gait_cb(gait_name, subgait_name, version, duration, gait_type):
        current_gait_pub.publish(gait=gait_name, subgait=subgait_name, version=version,
                                 duration=rospy.Duration.from_sec(duration), gait_type=gait_type)

    gait_state_machine.add_transition_callback(current_state_cb)
    gait_state_machine.add_gait_callback(current_gait_cb)

    gait_state_machine.run()
