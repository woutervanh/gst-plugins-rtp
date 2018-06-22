#!groovy
import groovy.json.JsonSlurper

@Library('jenkins-scripts@develop') _


pipeline {

  agent { label "linux-slave" }

  options {
    buildDiscarder(logRotator(numToKeepStr:'5'))
    timeout(time:1, unit: 'HOURS')
    timestamps()
  }

  environment {
    TF_VAR_ARTIFACTORY_APIKEY    = credentials('TF_VAR_ARTIFACTORY_APIKEY')
    TF_VAR_ARTIFACTORY_URL       = credentials('TF_VAR_ARTIFACTORY_URL')
    TF_VAR_ARTIFACTORY           = credentials('BARCO_USER')
    GPG_PRIVATE_EMS_KEY_SLM      = credentials('GPG_PRIVATE_EMS_SLM')
    GPG_PUBLIC_EMS_KEY_SLM       = credentials('GPG_PUBLIC_EMS_SLM')
    GPG_PRIVATE_NIDGR            = credentials('GPG_PRIVATE_NIDGR')
    GPG_PRIVATE_JONSP            = credentials('GPG_PRIVATE_KEY_JONSP')
    GPG_PRIVATE_KEY_SEBBI        = credentials('GPG_PRIVATE_KEY_sebbi')
    BIN_BARCO_COM_APIKEY         = credentials('BIN_BARCO_COM_APIKEY')
    EMS_SLACK_BASEURL            = credentials('EMS_SLACK_BASEURL')
    EMS_SLACK_AUTH_TOKEN         = credentials('EMS_SLACK_AUTH_TOKEN')
    EMS_SLACK_WORKSPACE          = credentials('EMS_SLACK_WORKSPACE')
    EMS_SLACK_CHANNEL            = credentials('EMS_SLACK_CHANNEL')
    ISO_GPG_KEY_PATH             = credentials('EMS_ISO_GPG_PRIVATE_KEY')
  }

  stages {
    stage ('env') {
      steps {
        sh 'make -f CI-Makefile env'
      }
    }

    stage ('package') {
      steps {
        sh 'make -f CI-Makefile package'
      }
    }


    stage ('lintian') {
          steps {
            sh 'make -f CI-Makefile lintian'
          }
    }

    stage ('test') {
          steps {
            sh 'make -f CI-Makefile test'
            step([$class: "TapPublisher", testResults: "**/results/test-results/*.tap"])
          }
    }

    stage ('iso') {
      when {
          anyOf {
            branch "debian/*";
            branch "master";
            changeRequest target : 'master'
          }
      }
      steps {
        sh 'make -f CI-Makefile iso'
        stash includes: 'results/iso/barco*.iso', name: 'iso'
      }
    }

    stage ('ova') {
      when {
          anyOf {
            branch "debian/*";
            branch "master";
            changeRequest target : 'master'
          }
      }
      agent { label "ova-generator" }
      steps {
        unstash 'iso'
        sh 'make -f CI-Makefile ova'
      }
    }

    stage ('publish'){
      when { branch "debian/*"   }
      steps {
        sh 'make -f CI-Makefile publish'
      }
    }
  }
 post {
    always {
      script{
         notifySlack2(currentBuild.result,
          "${EMS_SLACK_BASEURL}",
          "${EMS_SLACK_CHANNEL}",
          "${EMS_SLACK_WORKSPACE}",
          "${EMS_SLACK_AUTH_TOKEN}",
          "EMS_SLACK_AUTH_TOKEN"
        )
      }
    }
  }
}
