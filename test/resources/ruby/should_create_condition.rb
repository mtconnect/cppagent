MTConnect::Logger.info "Declaring class CreateCondition"

$trans =  MTConnect::RubyTransform.new("CreateCondition") do |event|
  dev = MTConnect.agent.default_device
  code, text = event.value.split(':').map { |s| s.strip }
  
  if code.start_with?('PLC')
    di = 'lp'
  elsif code.start_with?('NC')
    di = 'cmp'
  end
  
  di = dev.data_item(di)

  props = { VALUE: text, level: 'fault', nativeCode: code }
  
  obs = MTConnect::Observation.new(di, props)

  forward(obs)
end
