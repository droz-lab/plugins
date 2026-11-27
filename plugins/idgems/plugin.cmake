# Ressources du plugin : police Michroma (SIL OFL), embarquee via BinaryData.
juce_add_binary_data(IdgemsData SOURCES "${dir}/src/Resources/Fonts/Michroma-Regular.ttf")
target_link_libraries(Idgems PRIVATE IdgemsData)
