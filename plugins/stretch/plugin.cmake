# Ressources du plugin : police Michroma (SIL OFL), embarquee via BinaryData.
juce_add_binary_data(StretchData SOURCES src/Resources/Michroma-Regular.ttf)
target_link_libraries(IdgemsStretch PRIVATE StretchData)
