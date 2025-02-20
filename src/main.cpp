#include <Arduino.h>
#include <micro_ros_platformio.h>
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <std_msgs/msg/bool.h>
#include <std_msgs/msg/string.h>  // Message pour publier l'état de la LED

// Définir la pin de la LED
#define LED_PIN A0  // Pin PA0 de la STM32

// Variables Micro-ROS
rcl_node_t node;  // Nœud Micro-ROS
rcl_publisher_t led_state_publisher;  // Publisher pour l'état de la LED
rcl_subscription_t led_control_subscriber;  // Subscriber pour recevoir les commandes de la LED
rclc_executor_t executor;  // Executor pour gérer les tâches
rclc_support_t support;  // Support de Micro-ROS
rcl_allocator_t allocator;  // Allocateur
std_msgs__msg__Bool led_control_msg;  // Message de commande pour contrôler la LED
std_msgs__msg__String led_state_msg;  // Message pour publier l'état de la LED

// Fonction d'erreur en cas de problème
void error_loop() {
  while (1) {
    delay(100);  // Boucle infinie pour signaler l'erreur
  }
}

// Fonction pour assigner une chaîne à rosidl_runtime_c__String
void assign_string(rosidl_runtime_c__String * str, const char * data) {
  // Allouer de la mémoire pour la chaîne
  size_t len = strlen(data);
  str->data = (char*)malloc(len + 1);  // Ajouter un octet pour le caractère de fin '\0'
  
  if (str->data == NULL) {
    error_loop();  // Si l'allocation échoue, passer en boucle d'erreur
  }
  
  // Copier la chaîne dans le buffer alloué
  strcpy(str->data, data);
  str->size = len;  // Définir la taille
  str->capacity = len + 1;  // Réserver de l'espace pour '\0'
}

// Callback du subscriber pour contrôler la LED
void led_control_callback(const void * msgin) {
  const std_msgs__msg__Bool * msg = (const std_msgs__msg__Bool *)msgin;

  // Allumer ou éteindre la LED en fonction du message reçu
  if (msg->data) {
    digitalWrite(LED_PIN, HIGH);  // Allumer la LED
  } else {
    digitalWrite(LED_PIN, LOW);  // Éteindre la LED
  }

  // Allouer et assigner une chaîne à led_state_msg
  if (msg->data) {
    assign_string(&led_state_msg.data, "LED is ON");
  } else {
    assign_string(&led_state_msg.data, "LED is OFF");
  }

  // Publier l'état actuel de la LED
  rcl_publish(&led_state_publisher, &led_state_msg, NULL);  // Publier l'état de la LED
}

void setup() {
  // Initialisation de la communication série
  Serial.begin(115200);
  set_microros_serial_transports(Serial);  // Définir les transports série pour Micro-ROS
  
  // Attente initiale pour vérifier si l'agent est prêt
  unsigned long start_time = millis();
  while (millis() - start_time < 5000) {
    if (Serial.available()) {
      Serial.println("Agent detected, attempting connection...");
      break;  // On sort dès qu'on détecte une communication avec l'agent
    }
    delay(100);
  }

  // Initialiser la pin de la LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);  // Éteindre la LED au démarrage

  // Initialiser l'allocateur pour Micro-ROS
  allocator = rcl_get_default_allocator();

  // Initialiser le support Micro-ROS
  rcl_ret_t rc = rclc_support_init(&support, 0, NULL, &allocator);
  if (rc != RCL_RET_OK) {
    Serial.println("Failed to initialize support for Micro-ROS");
    error_loop();
  }

  // Initialiser le nœud Micro-ROS
  rc = rclc_node_init_default(&node, "stm32_led_node", "", &support);
  if (rc != RCL_RET_OK) {
    Serial.println("Failed to initialize node");
    error_loop();
  }

  // Initialiser le publisher pour l'état de la LED
  rc = rclc_publisher_init_default(
    &led_state_publisher,
    &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String),
    "led_state"
  );
  if (rc != RCL_RET_OK) {
    Serial.println("Failed to initialize publisher");
    error_loop();
  }

  // Initialiser le subscriber pour contrôler la LED
  rc = rclc_subscription_init_default(
    &led_control_subscriber,
    &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool),
    "led_control"
  );
  if (rc != RCL_RET_OK) {
    Serial.println("Failed to initialize subscriber");
    error_loop();
  }

  // Initialiser l'exécuteur pour gérer les abonnements et publications
  rc = rclc_executor_init(&executor, &support.context, 2, &allocator);  // 2 tâches : le publisher et le subscriber
  if (rc != RCL_RET_OK) {
    Serial.println("Failed to initialize executor");
    error_loop();
  }

  // Ajouter le subscriber à l'exécuteur
  rc = rclc_executor_add_subscription(&executor, &led_control_subscriber, &led_control_msg, &led_control_callback, ON_NEW_DATA);
  if (rc != RCL_RET_OK) {
    Serial.println("Failed to add subscription to executor");
    error_loop();
  }
}

void loop() {
  // Essayer de se connecter à l'agent de manière continue
  if (rclc_executor_spin_some(&executor, RCUTILS_MS_TO_NS(100)) != RCL_RET_OK) {
    Serial.println("Error during spin");
    error_loop();
  }

  // Exécuter périodiquement l'exécuteur Micro-ROS pour gérer les abonnements et publications
  delay(10);  // Petite pause pour permettre à l'exécuteur de tourner
}
