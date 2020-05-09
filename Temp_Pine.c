//Maxence Lord
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <time.h>
#include <aws_iot_config.h>
#include <aws_iot_log.h>
#include <aws_iot_version.h>
#include <aws_iot_mqtt_client_interface.h>

#define HOST_ADDRESS_SIZE 255

//IoT_Error_t aws_iot_mqtt_publish(AWS_IoT_Client *pClient, const char *pTopicName, uint16_t topicNameLen, IoT_Publish_Message_Params *pParams);
#define topic_pub "TempPine/sub"

/**
 * @brief Default cert location
 */
char certDirectory[PATH_MAX + 1] = "../../../certs";

/**
 * @brief Default MQTT HOST URL is pulled from the aws_iot_config.h
 */
char HostAddress[HOST_ADDRESS_SIZE] = AWS_IOT_MQTT_HOST;

/**
 * @brief Default MQTT port is pulled from the aws_iot_config.h
 */
uint32_t port = AWS_IOT_MQTT_PORT;

/**
 * @brief This parameter will avoid infinite loop of publish and exit the program after certain number of publishes
 */
uint32_t publishCount = 0;

void iot_subscribe_callback_handler(AWS_IoT_Client *pClient, char *topicName, uint16_t topicNameLen,
									IoT_Publish_Message_Params *params, void *pData) 
{
	IOT_UNUSED(pData);
	IOT_UNUSED(pClient);
	IOT_INFO("Subscribe callback");
	IOT_INFO("%.*s\t%.*s", topicNameLen, topicName, (int) params->payloadLen, (char *) params->payload);
}

void disconnectCallbackHandler(AWS_IoT_Client *pClient, void *data) 
{
	IOT_WARN("MQTT Disconnect");
	IoT_Error_t rc = FAILURE;

	if(NULL == pClient) 
    {
		return;
	}

	IOT_UNUSED(data);

	if(aws_iot_is_autoreconnect_enabled(pClient)) 
    {
		IOT_INFO("Auto Reconnect is enabled, Reconnecting attempt will start now");
	} 
    else 
    {
		IOT_WARN("Auto Reconnect not enabled. Starting manual reconnect...");
		rc = aws_iot_mqtt_attempt_reconnect(pClient);
		if(NETWORK_RECONNECTED == rc) {
			IOT_WARN("Manual Reconnect Successful");
		} 
        else 
        {
			IOT_WARN("Manual Reconnect Failed - %d", rc);
		}
	}
}

void parseInputArgsForConnectParams(int argc, char **argv) 
{
	int opt;

	while(-1 != (opt = getopt(argc, argv, "h:p:c:x:"))) 
    {
		switch(opt) 
        {
			case 'h':
				strncpy(HostAddress, optarg, HOST_ADDRESS_SIZE);
				IOT_DEBUG("Host %s", optarg);
				break;
			case 'p':
				port = atoi(optarg);
				IOT_DEBUG("arg %s", optarg);
				break;
			case 'c':
				strncpy(certDirectory, optarg, PATH_MAX + 1);
				IOT_DEBUG("cert root directory %s", optarg);
				break;
			case 'x':
				publishCount = atoi(optarg);
				IOT_DEBUG("publish %s times\n", optarg);
				break;
			case '?':
				if(optopt == 'c') 
                {
					IOT_ERROR("Option -%c requires an argument.", optopt);
				} 
                else if(isprint(optopt)) 
                {
					IOT_WARN("Unknown option `-%c'.", optopt);
				} 
                else 
                {
					IOT_WARN("Unknown option character `\\x%x'.", optopt);
				}
				break;
			default:
				IOT_ERROR("Error in command line argument parsing");
				break;
		}
	}

}

int main(int argc, char **argv) 
{
	bool infinitePublishFlag = true;
	char nowtime[1000];
	char rootCA[PATH_MAX + 1];
	char clientCRT[PATH_MAX + 1];
	char clientKey[PATH_MAX + 1];
	char CurrentWD[PATH_MAX + 1];
	char cPayload[100];
	
	int32_t i = 0;
	
	float TempPine; 
	float TempPineFin; 
    char file_name[40] = "/sys/class/thermal/thermal_zone0/temp"; 
    FILE *fp; 
	time_t t;
	
    
	IoT_Error_t rc = FAILURE;

	AWS_IoT_Client client;
	IoT_Client_Init_Params mqttInitParams = iotClientInitParamsDefault;
	IoT_Client_Connect_Params connectParams = iotClientConnectParamsDefault;

	IoT_Publish_Message_Params paramsQOS0;


	parseInputArgsForConnectParams(argc, argv);

	IOT_INFO("\nAWS IoT SDK Version %d.%d.%d-%s\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

	getcwd(CurrentWD, sizeof(CurrentWD));
	snprintf(rootCA, PATH_MAX + 1, "%s/%s/%s", CurrentWD, certDirectory, AWS_IOT_ROOT_CA_FILENAME);
	snprintf(clientCRT, PATH_MAX + 1, "%s/%s/%s", CurrentWD, certDirectory, AWS_IOT_CERTIFICATE_FILENAME);
	snprintf(clientKey, PATH_MAX + 1, "%s/%s/%s", CurrentWD, certDirectory, AWS_IOT_PRIVATE_KEY_FILENAME);

	IOT_DEBUG("rootCA %s", rootCA);
	IOT_DEBUG("clientCRT %s", clientCRT);
	IOT_DEBUG("clientKey %s", clientKey);
	mqttInitParams.enableAutoReconnect = false; // We enable this later below
	mqttInitParams.pHostURL = HostAddress;
	mqttInitParams.port = port;
	mqttInitParams.pRootCALocation = rootCA;
	mqttInitParams.pDeviceCertLocation = clientCRT;
	mqttInitParams.pDevicePrivateKeyLocation = clientKey;
	mqttInitParams.mqttCommandTimeout_ms = 20000;
	mqttInitParams.tlsHandshakeTimeout_ms = 5000;
	mqttInitParams.isSSLHostnameVerify = true;
	mqttInitParams.disconnectHandler = disconnectCallbackHandler;
	mqttInitParams.disconnectHandlerData = NULL;

	rc = aws_iot_mqtt_init(&client, &mqttInitParams);
	if(SUCCESS != rc) 
    {
		IOT_ERROR("aws_iot_mqtt_init returned error : %d ", rc);
		return rc;
	}

	connectParams.keepAliveIntervalInSec = 600;
	connectParams.isCleanSession = true;
	connectParams.MQTTVersion = MQTT_3_1_1;
	connectParams.pClientID = AWS_IOT_MQTT_CLIENT_ID;
	connectParams.clientIDLen = (uint16_t) strlen(AWS_IOT_MQTT_CLIENT_ID);
	connectParams.isWillMsgPresent = false;

	IOT_INFO("Connecting...");
	rc = aws_iot_mqtt_connect(&client, &connectParams);
	if(SUCCESS != rc) 
    {
		IOT_ERROR("Error(%d) connecting to %s:%d", rc, mqttInitParams.pHostURL, mqttInitParams.port);
		return rc;
	}
  
	rc = aws_iot_mqtt_autoreconnect_set_status(&client, true);
	if(SUCCESS != rc) 
    {
		IOT_ERROR("Unable to set Auto Reconnect to true - %d", rc);
		return rc;
	}

	IOT_INFO("Subscribing...");
	rc = aws_iot_mqtt_subscribe(&client, topic_pub, 12, QOS0, iot_subscribe_callback_handler, NULL);
	if(SUCCESS != rc) 
    {
		IOT_ERROR("Error subscribing : %d ", rc);
		return rc;
	}


	paramsQOS0.qos = QOS0;
	paramsQOS0.payload = (void *) cPayload;
	paramsQOS0.isRetained = 0;

	

	if(publishCount != 0) 
    {
		infinitePublishFlag = false;
	}

	while((NETWORK_ATTEMPTING_RECONNECT == rc || NETWORK_RECONNECTED == rc || SUCCESS == rc) && (publishCount > 0 || infinitePublishFlag)) 
    {
		fp = fopen(file_name, "r"); 

		fscanf(fp, "%f", &TempPine);

		fclose(fp);


		TempPineFin = TempPine / 1000;
		
		time_t t = time(NULL);



    	struct tm * p = localtime(&t);




    	strftime(nowtime, 1000, "%B %d %Y %X", p);



        sprintf(cPayload,"{\n" "\"timestamp\" :" "\"%s\",\n" "\"celsius\" :" "\"%f\"\n}",nowtime,TempPineFin);

		//Max time the yield function will wait for read messages
		rc = aws_iot_mqtt_yield(&client, 100);
		if(NETWORK_ATTEMPTING_RECONNECT == rc) 
        {
			// If the client is attempting to reconnect we will skip the rest of the loop.
			continue;
		}
		
		paramsQOS0.payloadLen = strlen(cPayload);
		rc = aws_iot_mqtt_publish(&client, topic_pub, 12, &paramsQOS0);
		if(publishCount > 0) 
        {
			publishCount--;
		}

		if(publishCount == 0 && !infinitePublishFlag) 
        {
			break;
		}

		IOT_INFO("-->sleep");
		sleep(60);
	}

	// Wait for all the messages to be received
	aws_iot_mqtt_yield(&client, 100);

	if(SUCCESS != rc) 
    {
		IOT_ERROR("An error occurred in the loop.\n");
	} else {
		IOT_INFO("Publish done\n");
	}

	return rc;
}