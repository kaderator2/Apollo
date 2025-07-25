<script setup>
import { ref, onMounted } from 'vue'
import Checkbox from '../../Checkbox.vue'

const props = defineProps({
  platform: String,
  config: Object,
  globalPrepCmd: Array,
  globalStateCmd: Array,
  serverCmd: Array
})

const config = ref(props.config)
const globalPrepCmd = ref(props.globalPrepCmd)
const globalStateCmd = ref(props.globalStateCmd)
const serverCmd = ref(props.serverCmd)

const cmds = ref({
  prep: globalPrepCmd,
  state: globalStateCmd
})

const prepCmdTemplate = {
  do: "",
  undo: "",
}

const serverCmdTemplate = {
  name: "",
  cmd: ""
}

function addCmd(cmdArr, template, idx) {
  const _tpl = Object.assign({}, template);

  if (props.platform === 'windows') {
    _tpl.elevated = false;
  }
  if (idx < 0) {
    cmdArr.push(_tpl);
  } else {
    cmdArr.splice(idx, 0, _tpl);
  }
}

function removeCmd(cmdArr, index) {
  cmdArr.splice(index,1)
}

onMounted(() => {
  // Set default value for enable_pairing if not present
  if (config.value.enable_pairing === undefined) {
    config.value.enable_pairing = "enabled"
  }
})
</script>

<template>
  <div id="general" class="config-page">
    <!-- Locale -->
    <div class="mb-3">
      <label for="locale" class="form-label">{{ $t('config.locale') }}</label>
      <select id="locale" class="form-select" v-model="config.locale">
        <option value="bg">Български (Bulgarian)</option>
        <option value="cs">Čeština (Czech)</option>
        <option value="de">Deutsch (German)</option>
        <option value="en">English</option>
        <option value="en_GB">English, UK</option>
        <option value="en_US">English, US</option>
        <option value="es">Español (Spanish)</option>
        <option value="fr">Français (French)</option>
        <option value="it">Italiano (Italian)</option>
        <option value="ja">日本語 (Japanese)</option>
        <option value="ko">한국어 (Korean)</option>
        <option value="pl">Polski (Polish)</option>
        <option value="pt">Português (Portuguese)</option>
        <option value="pt_BR">Português, Brasileiro (Portuguese, Brazilian)</option>
        <option value="ru">Русский (Russian)</option>
        <option value="sv">svenska (Swedish)</option>
        <option value="tr">Türkçe (Turkish)</option>
        <option value="uk">Українська (Ukranian)</option>
        <option value="zh">简体中文 (Chinese Simplified)</option>
        <option value="zh_TW">繁體中文 (Chinese Traditional)</option>
      </select>
      <div class="form-text">{{ $t('config.locale_desc') }}</div>
    </div>

    <!-- Apollo Name -->
    <div class="mb-3">
      <label for="sunshine_name" class="form-label">{{ $t('config.sunshine_name') }}</label>
      <input type="text" class="form-control" id="sunshine_name" placeholder="Apollo"
             v-model="config.sunshine_name" />
      <div class="form-text">{{ $t('config.sunshine_name_desc') }}</div>
    </div>

    <!-- Log Level -->
    <div class="mb-3">
      <label for="min_log_level" class="form-label">{{ $t('config.log_level') }}</label>
      <select id="min_log_level" class="form-select" v-model="config.min_log_level">
        <option value="0">{{ $t('config.log_level_0') }}</option>
        <option value="1">{{ $t('config.log_level_1') }}</option>
        <option value="2">{{ $t('config.log_level_2') }}</option>
        <option value="3">{{ $t('config.log_level_3') }}</option>
        <option value="4">{{ $t('config.log_level_4') }}</option>
        <option value="5">{{ $t('config.log_level_5') }}</option>
        <option value="6">{{ $t('config.log_level_6') }}</option>
      </select>
      <div class="form-text">{{ $t('config.log_level_desc') }}</div>
    </div>

    <!-- Global Prep/State Commands -->
    <div v-for="type in ['prep', 'state']" :id="`global_${type}_cmd`" class="mb-3 d-flex flex-column">
      <label class="form-label">{{ $t(`config.global_${type}_cmd`) }}</label>
      <div class="form-text pre-wrap">{{ $t(`config.global_${type}_cmd_desc`) }}</div>
      <table class="table" v-if="cmds[type].length > 0">
        <thead>
        <tr>
          <th scope="col"><i class="fas fa-play"></i> {{ $t('_common.do_cmd') }}</th>
          <th scope="col"><i class="fas fa-undo"></i> {{ $t('_common.undo_cmd') }}</th>
          <th scope="col" v-if="platform === 'windows'">
            <i class="fas fa-shield-alt"></i> {{ $t('_common.run_as') }}
          </th>
          <th scope="col"></th>
        </tr>
        </thead>
        <tbody>
        <tr v-for="(c, i) in cmds[type]">
          <td>
            <input type="text" class="form-control monospace" v-model="c.do" />
          </td>
          <td>
            <input type="text" class="form-control monospace" v-model="c.undo" />
          </td>
          <td v-if="platform === 'windows'" class="align-middle">
            <Checkbox :id="type + '-cmd-admin-' + i"
                      label="_common.elevated"
                      desc=""
                      default="false"
                      v-model="c.elevated"
            ></Checkbox>
          </td>
          <td class="text-end">
            <button class="btn btn-danger me-2" @click="removeCmd(cmds[type], i)">
              <i class="fas fa-trash"></i>
            </button>
            <button class="btn btn-success" @click="addCmd(cmds[type], prepCmdTemplate, i)">
              <i class="fas fa-plus"></i>
            </button>
          </td>
        </tr>
        </tbody>
      </table>
      <button class="ms-0 mt-2 btn btn-success" style="margin: 0 auto" @click="addCmd(cmds[type], prepCmdTemplate, -1)">
        &plus; {{ $t('config.add') }}
      </button>
    </div>

    <!-- Server Commands -->
    <div id="server_cmd" class="mb-3 d-flex flex-column">
      <label class="form-label">{{ $t('config.server_cmd') }}</label>
      <div class="form-text">{{ $t('config.server_cmd_desc') }}</div>
      <div class="form-text">
        <a href="https://github.com/ClassicOldSong/Apollo/wiki/Server-Commands" target="_blank">{{ $t('_common.learn_more') }}</a>
      </div>
      <table class="table" v-if="serverCmd.length > 0">
        <thead>
        <tr>
          <th scope="col"><i class="fas fa-tag"></i> {{ $t('_common.cmd_name') }}</th>
          <th scope="col"><i class="fas fa-terminal"></i> {{ $t('_common.cmd_val') }}</th>
          <th scope="col" v-if="platform === 'windows'">
            <i class="fas fa-shield-alt"></i> {{ $t('_common.run_as') }}
          </th>
          <th scope="col"></th>
        </tr>
        </thead>
        <tbody>
        <tr v-for="(c, i) in serverCmd">
          <td>
            <input type="text" class="form-control" v-model="c.name" />
          </td>
          <td>
            <input type="text" class="form-control monospace" v-model="c.cmd" />
          </td>
          <td v-if="platform === 'windows'">
            <div class="form-check">
              <input type="checkbox" class="form-check-input" :id="'server-cmd-admin-' + i" v-model="c.elevated"/>
              <label :for="'server-cmd-admin-' + i" class="form-check-label">{{ $t('_common.elevated') }}</label>
            </div>
          </td>
          <td class="text-end">
            <button class="btn btn-danger me-2" @click="removeCmd(serverCmd, i)">
              <i class="fas fa-trash"></i>
            </button>
            <button class="btn btn-success" @click="addCmd(serverCmd, serverCmdTemplate, i)">
              <i class="fas fa-plus"></i>
            </button>
          </td>
        </tr>
        </tbody>
      </table>
      <button class="ms-0 mt-2 btn btn-success" style="margin: 0 auto" @click="addCmd(serverCmd, serverCmdTemplate, -1)">
        &plus; {{ $t('config.add') }}
      </button>
    </div>

    <!-- Enable Pairing -->
    <Checkbox class="mb-3"
              id="enable_pairing"
              locale-prefix="config"
              v-model="config.enable_pairing"
              default="true"
    ></Checkbox>

    <!-- Enable Discovery -->
    <Checkbox class="mb-3"
              id="enable_discovery"
              locale-prefix="config"
              v-model="config.enable_discovery"
              default="true"
    ></Checkbox>

    <!-- Hide Tray Controls -->
    <Checkbox class="mb-3"
              id="hide_tray_controls"
              locale-prefix="config"
              v-model="config.hide_tray_controls"
              default="false"
    ></Checkbox>

    <!-- Notify Pre-Releases -->
    <Checkbox class="mb-3"
              id="notify_pre_releases"
              locale-prefix="config"
              v-model="config.notify_pre_releases"
              default="false"
    ></Checkbox>
  </div>
</template>

<style scoped>

</style>
