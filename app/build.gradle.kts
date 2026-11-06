plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.cardputer.cardulator"
    compileSdk = 36

    defaultConfig {
        applicationId = "com.cardputer.cardulator"
        minSdk = 24
        targetSdk = 36
        versionCode = (System.getenv("VERSION_CODE") ?: project.findProperty("versionCode")?.toString() ?: "1").toInt()
        val appVersionName = System.getenv("VERSION_NAME") ?: (project.findProperty("versionName") as? String ?: "26.10.25")
        versionName = appVersionName

        externalNativeBuild {
            cmake {
                cppFlags += "-std=c++20"
                arguments += "-DANDROID_STL=c++_shared"
            }
        }
    }

    splits {
        abi {
            isEnable = true
            reset()
            include("arm64-v8a", "armeabi-v7a", "x86_64", "x86")
            isUniversalApk = true
        }
    }

    signingConfigs {
        create("release") {
            val keystorePath = System.getenv("RELEASE_KEYSTORE_PATH") ?: "release.jks"
            val keystorePasswordEnv = System.getenv("RELEASE_KEYSTORE_PASSWORD") ?: ""
            val keyAliasEnv = System.getenv("RELEASE_KEY_ALIAS") ?: ""
            val keyPasswordEnv = System.getenv("RELEASE_KEY_PASSWORD") ?: ""

            if (file(keystorePath).exists() && listOf(keystorePasswordEnv, keyAliasEnv, keyPasswordEnv).all { it.isNotBlank() }) {
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
    implementation("androidx.lifecycle:lifecycle-viewmodel:2.8.7")
    testImplementation("junit:junit:4.13.2")
}

// Validate only release artifacts; debug and CI builds do not need release secrets.
val validateRelease by tasks.registering {
    doLast {
        require(android.defaultConfig.versionCode!! > 1) { "Set VERSION_CODE or -PversionCode to a unique, increasing release version code" }
        require(android.signingConfigs.getByName("release").isSigningReady) { "Release signing requires a keystore and all RELEASE_KEYSTORE_* / RELEASE_KEY_* credentials" }
    }
}
tasks.configureEach {
    if (name == "preReleaseBuild") dependsOn(validateRelease)
}
