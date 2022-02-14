from enum import Enum

class SignalingChanelType(Enum):
    SINGLE_MASTER = 1
    FULL_MESH = 2

class SignalingChannelStatus(Enum):
    CREATING = 1
    ACTIVE = 2
    UPDATING = 3
    DELETING = 4

class SignalingChanel:
    def __init__(self, name, status, channelType, messageTtlSeconds):
        self.channelName = name
        self.channelArn = 'arn:aws:kinesisvideo:us-west-2:302319298006:channel/' + name + '/1641534227750'
        self.channelStatus = status
        self.channelType = channelType
        self.signalingMasterMessageTtlSeconds = messageTtlSeconds
        self.creationTime = '1.64153422775E9'
        self.channelTags = {}

    def getChannelInfo(self):
        channelInfo ={}
        channelInfo['ChannelARN'] = self.channelArn
        channelInfo['ChannelName'] = self.channelName

        if self.channelStatus == SignalingChannelStatus.ACTIVE:
            channelInfo['ChannelStatus'] = 'ACTIVE'
        if self.channelStatus == SignalingChannelStatus.CREATING:
            channelInfo['ChannelStatus'] = 'CREATING'
        if self.channelStatus == SignalingChannelStatus.UPDATING:
            channelInfo['ChannelStatus'] = 'UPDATING'
        if self.channelStatus == SignalingChannelStatus.DELETING:
            channelInfo['ChannelStatus'] = 'DELETING'

        if self.channelType == SignalingChanelType.SINGLE_MASTER:
            channelInfo['ChannelType'] = 'SINGLE_MASTER'
        if self.channelType == SignalingChanelType.FULL_MESH:
            channelInfo['ChannelType'] = 'FULL_MESH'

        channelInfo['CreationTime'] = self.creationTime

        signalingMasterConfiguration = {}
        signalingMasterConfiguration['MessageTtlSeconds'] = self.signalingMasterMessageTtlSeconds
        channelInfo['SingleMasterConfiguration'] = signalingMasterConfiguration

        channelInfo['Version'] = 'IjGbnoI0QAoeLD1HXffc'

        return channelInfo


