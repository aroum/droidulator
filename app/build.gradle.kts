plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.cardputer.cardulator"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.cardputer.cardulator"
        minSdk = 24
        targetSdk = 34
        versionCode = 1
        versionName = "26.08.16"

        externalNativeBuild {
            cmake {
                cppFlags += "-std=c++20"
                arguments += "-DANDROID_STL=c++_shared"
            }
        }
    }

    signingConfigs {
        create("release") {
            val keystorePath = System.getenv("RELEASE_KEYSTORE_PATH") ?: "release.jks"
            val keystorePasswordEnv = System.getenv("RELEASE_KEYSTORE_PASSWORD") ?: ""
            val keyAliasEnv = System.getenv("RELEASE_KEY_ALIAS") ?: ""
            val keyPasswordEnv = System.getenv("RELEASE_KEY_PASSWORD") ?: ""

            if (file(keystorePath).exists() && keystorePasswordEnv.isNotEmpty()) {
                storeFile = file(keystorePath)
                storePassword = keystorePasswordEnv
                keyAlias = keyAliasEnv
                keyPassword = keyPasswordEnv
            }
        }
    }

    buildTypes {
        getByName("release") {
            isMinifyEnabled = false
            signingConfig = signingConfigs.getByName("release")
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
        create("ci") {
            isDebuggable = true
            val relStore = signingConfigs.getByName("release").storeFile
            if (relStore != null && relStore.exists()) {
                signingConfig = signingConfigs.getByName("release")
            } else {
                signingConfig = signingConfigs.getByName("debug")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.12.0")
    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("com.google.android.material:material:1.11.0")
    testImplementation("junit:junit:4.13.2")
}
