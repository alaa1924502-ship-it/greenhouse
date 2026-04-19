#define BLYNK_AUTH_TOKEN "pdNfdI5Jilv4Rq69WIQdKPYO2pWULxR8"
#define FIREBASE_HOST "greenhouse-27fd6-default-rtdb.firebaseio.com"  // مصحح
#define FIREBASE_AUTH "01GUpPKIcN7pPmJqw9JsSUOdXuWp7yASSiUKdUzz"

void setup() {
  // ... hardware ...
  
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);  // مصحح
  
  Firebase.begin(&config, &auth);  // مصحح
  
  Serial.println("✅ READY!");
}