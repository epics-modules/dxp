# Setup the DXP CAMAC MCA.
# Set logging level (1=ERROR, 2=WARNING, 3=INFO, 4=DEBUG)
xiaSetLogLevel(2)
xiaInit("16element_reset.ini")
xiaStartSystem()

# DXPConfig(serverName, nchans)
DXPConfig("DXP1", 16)

# Load all of the MCA and DXP records
dbLoadTemplate("16element.template")
