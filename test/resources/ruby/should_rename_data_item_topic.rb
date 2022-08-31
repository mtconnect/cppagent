
alarm = MTConnect.agent.data_item_for_device('000', 'a')
alarm.topic = alarm.topic.sub(/Event/, 'State')

block = MTConnect.agent.data_item_for_device('000', 'block')
block.topic = block.topic.sub(/Event/, 'State')

mode = MTConnect.agent.data_item_for_device('000', 'mode')
mode.topic = mode.topic.gsub(/([A-Za-z]+)\[([A-Za-z]+)\]/, '\1:\2')


