import pyrebase
import cv2
import urllib.request
import numpy as np
import time
import tensorflow_hub as hub
import pandas as pd
import tensorflow as tf
import matplotlib.pyplot as plt

# Apply image detector on a batch of image.
detector = hub.load("https://tfhub.dev/tensorflow/efficientdet/lite2/detection/1")

firebaseConfig = {
    "apiKey"            : "AIzaSyDkDXEzV5L6OxDSd7CFiQwrGEBuQeg59-E",
    "authDomain"        : "espcam-9edcb.firebaseapp.com",
    "databaseURL"       : "https://espcam-9edcb-default-rtdb.firebaseio.com/",
    "projectId"         : "espcam-9edcb",
    "storageBucket"     : "espcam-9edcb.appspot.com",
    "messagingSenderId" : "851470719394",
    "appId"             : "1:851470719394:web:87be0f43da852b7f8abafb"
};

firebase = pyrebase.initialize_app(firebaseConfig)
auth = firebase.auth()
db = firebase.database()
storage = firebase.storage()

#storage.child("images/koo2.png").put("Lenna_100.png")

url = db.child("/serverUrl").get().val()  # recebe a URL a qual a ESPCAN está enviando as imagens
print(url)

#Now you can use rgb_tensor to predict label for exemple :

width = 800
height = 600
while True:
    url = db.child("/serverUrl").get().val()  # recebe a URL a qual a ESPCAN está enviando as imagens
    print(url)

    pir = db.child("/presence").get().val()  # recebe a URL a qual a ESPCAN está enviando as imagens
    print(pir)

    if pir:
        print("Movimento detectado!")
        print("Solicitando imagem e iniciando processamento...")
        try:
            img_resp = urllib.request.urlopen(url)
            img = np.array(bytearray(img_resp.read()), dtype=np.uint8)

            #Load image
            img = cv2.imdecode(img, -1)
            inp = cv2.resize(img, (width, height))

            #Convert img to RGB
            rgb = cv2.cvtColor(inp, cv2.COLOR_BGR2RGB)

            #Is optional but i recommend (float convertion and convert img to tensor image)
            rgb_tensor = tf.convert_to_tensor(rgb, dtype=tf.uint8)

            #Add dims to rgb_tensor
            rgb_tensor = tf.expand_dims(rgb_tensor, 0)

            #plt.figure(figsize=(10,10))
            #plt.imshow(rgb)
            #plt.show()

            boxes, scores, classes, num_detections = detector(rgb_tensor)

            labels = pd.read_csv('labels.csv', sep=';', index_col='ID')
            labels = labels['OBJECT (2017 REL.)']
            labels.head()

            pred_labels = classes.numpy().astype('int')[0]
            pred_labels = [labels[i] for i in pred_labels]
            pred_boxes = boxes.numpy()[0].astype('int')
            pred_scores = scores.numpy()[0]

            for score, (ymin, xmin, ymax, xmax), label in zip(pred_scores, pred_boxes, pred_labels):
                if score < 0.65:
                    continue
                if label == "person":
                    score_txt = f'{100 * round(score)}%'
                    img_boxes = cv2.rectangle(rgb, (xmin, ymax), (xmax, ymin), (0, 255, 0), 2)
                    font = cv2.FONT_HERSHEY_SIMPLEX
                    cv2.putText(img_boxes, label, (xmin, ymax - 10), font, 1.5, (255, 0, 0), 2, cv2.LINE_AA)
                    print(label)
                    print(score)
                    cv2.putText(img_boxes, "", (xmax, ymax - 10), font, 1.5, (255, 0, 0), 2, cv2.LINE_AA)
                    break
            if label == "person":
                plt.figure(figsize=(10, 10))
                plt.imshow(img_boxes)
                plt.axis('off')
                plt.plot()
                ##plt.show()
                #plt.savefig('captura.png', transparent=True, )
                plt.savefig('captura.png', bbox_inches='tight')

                db.child("/person_detected").set(True)

                time.sleep(2)
                storage.child("images/captura.png").put("captura.png")
            else:
                print("Pessoa não detectada.")
        except:
            print("Pessoa não detectada.")
            print("Possível erro no processamento ou leitura da imagem.")
    else:
        print("Aguardando detecção de movimento...")
    time.sleep(3)