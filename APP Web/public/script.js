// Variables de estado de la aplicación
let appState = {
  isConnected: false,
  lastUpdateTime: null,
  connectionAttempts: 0,
  rawData: null,
  debugMode: false
};

// Elementos del DOM que usaremos con frecuencia
const domElements = {
  loadingOverlay: document.getElementById('loading-overlay'),
  connectionStatus: {
    container: document.getElementById('connection-status'),
    dot: document.querySelector('.status-dot'),
    text: document.querySelector('.status-text')
  },
  errorMessage: document.getElementById('error'),
  lastUpdate: document.getElementById('last-update'),
  debugInfo: document.getElementById('debug-info'),
  debugData: document.getElementById('debug-data'),
  toggleDebug: document.getElementById('toggle-debug')
};

// Configuración de Firebase
const firebaseConfig = {
  apiKey: "AIzaSyBbgHvC5AHPUfDYQ5nfgSyrK5TRo-p3_8w",
  authDomain: "aircare-8aed5.firebaseapp.com",
  databaseURL: "https://aircare-8aed5-default-rtdb.europe-west1.firebasedatabase.app",
  projectId: "aircare-8aed5",
  storageBucket: "aircare-8aed5.firebasestorage.app",
  messagingSenderId: "712319210734",
  appId: "1:712319210734:web:4327671b3fd926373a4fff"
};

// Inicializar conexión con Firebase
function initializeFirebase() {
  try {
    // Verificar que firebase está definido
    if (typeof firebase === 'undefined') {
      throw new Error('Firebase no está definida. Las bibliotecas no se cargaron correctamente.');
    }

    // Inicializar la aplicación
    firebase.initializeApp(firebaseConfig);
    
    // Obtener referencia a la base de datos
    const database = firebase.database();
    
    // Configurar la persistencia para mejor experiencia offline
    database.goOnline();
    
    log('Firebase inicializado correctamente');
    
    // Configurar listeners para los datos
    setupDataListeners(database);
    
    // Configurar comprobación de conexión
    setupConnectionMonitoring(database);
    
    return database;
  } catch (error) {
    handleError('Error al inicializar Firebase', error);
    return null;
  }
}

// Configurar listeners para los datos de sensores
function setupDataListeners(database) {
  if (!database) return;

  // Referencia a sensores
  const sensoresRef = database.ref('aircare/sensores');
  const sensoresRef1 = database.ref('aircare/nodo1');
  const sensoresRef2 = database.ref('aircare/nodo2');
  
  // Listener para cambios en datos de sensores
  sensoresRef.on('value', (snapshot) => {
    handleDataUpdate(snapshot.val());
  }, (error) => {
    handleError('Error al leer datos de sensores', error);
  });

  // Listener para cambios en datos de nodo1
  sensoresRef1.on('value', (snapshot) => {
    handleDataUpdate(snapshot.val());
  }, (error) => {
    handleError('Error al leer datos de sensores', error);
  });


  // Listener para cambios en datos de nodo2
  sensoresRef2.on('value', (snapshot) => {
    handleDataUpdate(snapshot.val());
  }, (error) => {
    handleError('Error al leer datos de sensores', error);
  });

  // Referencia a última actualización
  const lastUpdateRef = database.ref('aircare/last_update');
  
  // Listener para cambios en última actualización
  lastUpdateRef.on('value', (snapshot) => {
    updateLastUpdateTime(snapshot.val());
  }, (error) => {
    log('Error al leer timestamp de última actualización', error);
  });
}

// Manejar los datos recibidos de sensores
function handleDataUpdate(data) {
  if (!data) {
    showError('No se recibieron datos de los sensores');
    return;
  }
  
  log('Datos de sensores recibidos', data);
  appState.rawData = data;
  updateDebugInfo();
  
  // Actualizar valores en la UI
  updateSensorValue('temperature-value', data.temperature);
  updateSensorValue('humidity-value', data.humidity);
  updateSensorValue('co2-value', data.co2);
  updateSensorValue('pm1-value', data.pm1);
  updateSensorValue('pm2_5-value', data.pm2_5);
  updateSensorValue('pm4-value', data.pm4);
  updateSensorValue('pm10-value', data.pm10);
  updateSensorValue('tvoc-value', data.tvoc);
  updateSensorValue('filtro-value', data.filtro_command);
  updateSensorValue('ventilator-value', data.ventilator_command);

  
  // Mostrar que estamos conectados
  setConnectionStatus('connected');
  
  // Ocultar la pantalla de carga si aún está visible
  hideLoadingOverlay();
}

// Configurar monitoreo de conexión
function setupConnectionMonitoring(database) {
  if (!database) return;
  
  const connectedRef = database.ref('.info/connected');
  connectedRef.on('value', (snap) => {
    if (snap.val() === true) {
      appState.isConnected = true;
      setConnectionStatus('connected');
      log('Conectado a Firebase');
    } else {
      appState.isConnected = false;
      setConnectionStatus('disconnected');
      log('Desconectado de Firebase');
    }
  });
}

// Actualizar el valor de un sensor en la interfaz
function updateSensorValue(id, value) {
  const element = document.getElementById(id);
  if (!element) {
    log(`Elemento no encontrado: ${id}`);
    return;
  }
  
  // Limpiar cualquier estado de carga
  const card = element.closest('.sensor-card');
  if (card) card.classList.remove('loading');
  
  // Mostrar valor o placeholder si no hay datos
  if (value !== undefined && value !== null) {
    if (typeof value === 'number') {
      // Para valores numéricos, formateamos según el tipo de sensor
      if (id === 'co2-value' || id === 'tvoc-value') {
        // Valores enteros para CO2 y TVOC
        element.textContent = Math.round(value);
      } else {
        // Un decimal para el resto
        element.textContent = value.toFixed(1);
      }
    } else {
      // Para valores no numéricos
      element.textContent = value;
    }
  } else {
    element.textContent = '--';
    if (card) card.classList.add('loading');
  }
}

// Actualizar última hora de actualización
function updateLastUpdateTime(timestamp) {
  if (!timestamp) return;
  
  appState.lastUpdateTime = timestamp;
  updateDebugInfo();
  
  const formattedTime = formatTimestamp(timestamp);
  if (domElements.lastUpdate) {
    domElements.lastUpdate.textContent = formattedTime;
  }
}

// Formatear timestamp para mostrar
function formatTimestamp(timestamp) {
  if (!timestamp) return '--/--/---- --:--:--';
  
  try {
    const date = new Date(parseInt(timestamp));
    return date.toLocaleString('es-ES', {
      day: '2-digit',
      month: '2-digit',
      year: 'numeric',
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit'
    });
  } catch (error) {
    log('Error al formatear timestamp', error);
    return timestamp.toString();
  }
}

// Establecer estado de conexión en la UI
function setConnectionStatus(status) {
  if (!domElements.connectionStatus.dot || !domElements.connectionStatus.text) return;
  
  // Limpiar clases existentes
  domElements.connectionStatus.dot.classList.remove('connected', 'disconnected', 'connecting');
  
  // Establecer nueva clase y texto
  switch (status) {
    case 'connected':
      domElements.connectionStatus.dot.classList.add('connected');
      domElements.connectionStatus.text.textContent = 'Conectado';
      hideLoadingOverlay();
      break;
    case 'disconnected':
      domElements.connectionStatus.dot.classList.add('disconnected');
      domElements.connectionStatus.text.textContent = 'Desconectado';
      break;
    case 'connecting':
      domElements.connectionStatus.dot.classList.add('connecting');
      domElements.connectionStatus.text.textContent = 'Conectando...';
      break;
    default:
      domElements.connectionStatus.text.textContent = status;
  }
}

// Mostrar mensaje de error
function showError(message) {
  if (!domElements.errorMessage) return;
  
  domElements.errorMessage.textContent = message;
  domElements.errorMessage.style.display = 'block';
  log('Error mostrado:', message);
}

// Ocultar mensaje de error
function hideError() {
  if (!domElements.errorMessage) return;
  
  domElements.errorMessage.style.display = 'none';
}

// Manejar errores generales
function handleError(context, error) {
  const errorMessage = `${context}: ${error.message || error}`;
  log(errorMessage, error);
  
  showError(errorMessage);
  setConnectionStatus('disconnected');
  
  // Si es un error de conexión, ocultar la pantalla de carga después de un tiempo
  setTimeout(hideLoadingOverlay, 3000);
}

// Ocultar pantalla de carga
function hideLoadingOverlay() {
  if (!domElements.loadingOverlay) return;
  
  domElements.loadingOverlay.style.opacity = '0';
  setTimeout(() => {
    domElements.loadingOverlay.style.display = 'none';
  }, 500);
}

// Actualizar información de depuración
function updateDebugInfo() {
  if (!appState.debugMode || !domElements.debugData) return;
  
  const debugInfo = {
    connectionStatus: appState.isConnected ? 'Conectado' : 'Desconectado',
    lastUpdateTime: appState.lastUpdateTime ? new Date(parseInt(appState.lastUpdateTime)).toISOString() : null,
    connectionAttempts: appState.connectionAttempts,
    rawData: appState.rawData
  };
  
  domElements.debugData.textContent = JSON.stringify(debugInfo, null, 2);
}

// Función para registrar eventos y errores
function log(message, data) {
  const timestamp = new Date().toLocaleTimeString();
  const logMessage = `[${timestamp}] ${message}`;
  
  console.log(logMessage, data !== undefined ? data : '');
  
  // Actualizar depuración si está activa
  updateDebugInfo();
}

// Configurar controles de depuración
function setupDebugControls() {
  if (!domElements.toggleDebug) return;
  
  domElements.toggleDebug.addEventListener('click', () => {
    appState.debugMode = !appState.debugMode;
    
    if (appState.debugMode) {
      domElements.debugInfo.style.display = 'block';
      domElements.toggleDebug.textContent = 'Ocultar información de depuración';
      updateDebugInfo();
    } else {
      domElements.debugInfo.style.display = 'none';
      domElements.toggleDebug.textContent = 'Mostrar información de depuración';
    }
  });
}

// Inicializar la aplicación cuando se carga la página
window.addEventListener('DOMContentLoaded', () => {
  try {
    log('Aplicación inicializándose');
    setConnectionStatus('connecting');
    
    // Configurar controles de depuración
    setupDebugControls();
    
    // Intentar inicializar Firebase
    const database = initializeFirebase();
    
    // Si Firebase no se inicializó correctamente
    if (!database) {
      handleError('Error crítico', new Error('No se pudo inicializar Firebase'));
    }
    
    // Configurar un tiempo límite para la pantalla de carga
    setTimeout(() => {
      if (!appState.isConnected) {
        hideLoadingOverlay();
        showError('Tiempo de espera agotado. No se pudo conectar con el servidor.');
      }
    }, 10000);
    
  } catch (error) {
    handleError('Error al inicializar la aplicación', error);
  }
});

// Manejar eventos de visibilidad para reconectar si es necesario
document.addEventListener('visibilitychange', () => {
  if (!document.hidden && !appState.isConnected) {
    log('Documento visible de nuevo, intentando reconectar...');
    setConnectionStatus('connecting');
    
    // Reintentar conexión
    appState.connectionAttempts++;
    firebase.database().goOnline();
  }
});