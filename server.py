import hmac

import hashlib

import json

from flask import Flask, request, jsonify

import requests
import logging


# Set up logging

logging.basicConfig(level=logging.DEBUG)

logger = logging.getLogger(__name__)


app = Flask(__name__)


# Secret key must match the one in ESP32 code

SECRET_KEY = "bcf35bfbe47d73b53007b904d5c54c313c70d7f5b5e3afb6b63f02a9640c8404"


# Blynk Configuration

BLYNK_AUTH_TOKEN = "hjEaS5i_NV19ZRnc11pFDyEmLRVKtndc"

BLYNK_API_URL = "https://blynk.cloud/external/api/batch/update"


@app.route('/', methods=['GET'])

def home():

    try:

        logger.info("GET request received at home endpoint")

        return jsonify({

            "status": "success",

            "message": "Server is running successfully"

        })

    except Exception as e:

        logger.error(f"Error in home route: {str(e)}")

        return jsonify({

            "status": "error",
            "message": str(e)

        }), 500


def verify_hmac(data, received_hmac):

    try:

        computed_hmac = hmac.new(

            SECRET_KEY.encode(), 

            data.encode(), 

            hashlib.sha256

        ).hexdigest()

        return hmac.compare_digest(computed_hmac, received_hmac)

    except Exception as e:

        logger.error(f"HMAC verification error: {str(e)}")

        return False


def send_to_blynk(temperature, humidity, gas_value):

    try:

        payload = {

            "token": BLYNK_AUTH_TOKEN,

            "v0": temperature,

            "v1": humidity,

            "v2": gas_value

        }

        response = requests.get(BLYNK_API_URL, params=payload)

        logger.info(f"Blynk Response: {response.text}")

        return response.status_code == 200

    except Exception as e:

        logger.error(f"Blynk send error: {str(e)}")

        return False


@app.route('/', methods=['POST'])

def receive_data():

    try:

        logger.info("POST request received")

        data = request.form.get('data')

        received_hmac = request.form.get('hmac')
        

        if not data or not received_hmac:

            logger.error("Missing data or HMAC")

            return jsonify({

                "status": "error",

                "message": "Missing data or HMAC"

            }), 400
        

        if not verify_hmac(data, received_hmac):

            logger.error("HMAC verification failed")

            return jsonify({

                "status": "error",

                "message": "HMAC verification failed"

            }), 403
        

        sensor_data = json.loads(data)

        temperature = sensor_data.get('temperature')

        humidity = sensor_data.get('humidity')

        gas_value = sensor_data.get('gasValue')
        

        if send_to_blynk(temperature, humidity, gas_value):

            return jsonify({

                "status": "success",

                "message": "Data processed and sent to Blynk"

            })
        else:

            return jsonify({

                "status": "error",

                "message": "Failed to send to Blynk"

            }), 500
            

    except json.JSONDecodeError as e:

        logger.error(f"JSON decode error: {str(e)}")

        return jsonify({

            "status": "error",

            "message": "Invalid JSON data"

        }), 400

    except Exception as e:

        logger.error(f"Server error: {str(e)}")

        return jsonify({

            "status": "error",

            "message": f"Internal server error: {str(e)}"

        }), 500

if __name__ == '__main__':
    try:

        logger.info("Starting Flask server...")

        app.run(host='0.0.0.0', port=80, debug=True)

    except Exception as e:

        logger.error(f"Server startup error: {str(e)}")