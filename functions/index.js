const functions = require('firebase-functions');
const admin = require('firebase-admin');

// Initialize the Admin SDK once
admin.initializeApp();

const firestore = admin.firestore();

/**
 * Cloud Function triggered by Realtime Database writes to a device's status.
 * It mirrors the new data to a document in the Firestore 'devices' collection.
 */
exports.mirrorDeviceStatusToFirestore = functions.database
  // Specify the path to watch in RTDB. Use a wildcard {deviceId}.
  .ref('/hydrolink/devices/{deviceId}/status')
  .onWrite(async (change, context) => {
    
    // Get the ID of the device that changed
    const deviceId = context.params.deviceId;
    
    // Check if the data was deleted (after) or if it's new/updated (after)
    const statusData = change.after.val();

    // Define the target path in Firestore
    const deviceDocRef = firestore.collection('hydrolink/devices').doc(deviceId);
    
    // --- Data Mirroring Logic ---

    // If the data was deleted in RTDB, delete the corresponding Firestore document
    if (!statusData) {
      functions.logger.log(`Deleting Firestore document for device: ${deviceId}`);
      return deviceDocRef.delete();
    }

    // Otherwise, write (or merge) the RTDB data into the Firestore document
    const dataToWrite = {
      status: statusData,
      lastUpdatedRTDB: admin.firestore.FieldValue.serverTimestamp() // Add a timestamp for tracking
    };

    functions.logger.log(`Mirroring status for device: ${deviceId}`, dataToWrite);

    // Use .set with { merge: true } to update only the 'status' field 
    // without overwriting other fields (like 'settings') in the device document.
    return deviceDocRef.set(dataToWrite, { merge: true });
  });